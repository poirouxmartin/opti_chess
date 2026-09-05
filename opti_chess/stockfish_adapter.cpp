#include "stockfish_adapter.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sstream>
#include <algorithm>

struct StockfishImpl {
    HANDLE hStd_IN_Wr = NULL;
    HANDLE hStd_OUT_Rd = NULL;
    PROCESS_INFORMATION pi = {};
    bool available = false;

    ~StockfishImpl() {
        if (available) {
            send_cmd("quit");
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        if (hStd_IN_Wr) CloseHandle(hStd_IN_Wr);
        if (hStd_OUT_Rd) CloseHandle(hStd_OUT_Rd);
    }

    void send_cmd(const std::string& cmd) {
        if (!available) return;
        DWORD written;
        std::string data = cmd + "\n";
        WriteFile(hStd_IN_Wr, data.c_str(), (DWORD)data.size(), &written, NULL);
    }

    std::string read_until(const std::string& token, int timeout_ms) {
        if (!available) return "";
        std::string result;
        char buf[4096];
        DWORD start = GetTickCount();
        while ((int)(GetTickCount() - start) < timeout_ms) {
            DWORD avail = 0;
            if (PeekNamedPipe(hStd_OUT_Rd, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                DWORD br = 0;
                if (ReadFile(hStd_OUT_Rd, buf, (std::min)(avail, (DWORD)(sizeof(buf) - 1)), &br, NULL) && br > 0) {
                    buf[br] = 0;
                    result += buf;
                    if (result.find(token) != std::string::npos) return result;
                }
            } else {
                Sleep(10);
            }
        }
        return result;
    }
};

static int parse_int_after(const std::string& s, const std::string& key) {
    size_t pos = s.find(key);
    if (pos == std::string::npos) return 0;
    pos += key.size();
    size_t end = s.find(' ', pos);
    if (end == std::string::npos) end = s.size();
    return std::stoi(s.substr(pos, end - pos));
}

StockfishAdapter::StockfishAdapter() : _impl(nullptr) {}

StockfishAdapter::StockfishAdapter(const std::string& exe_path)
    : _impl(std::make_unique<StockfishImpl>()) {

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE hStd_OUT_Wr = NULL, hStd_IN_Rd = NULL;
    if (!CreatePipe(&_impl->hStd_OUT_Rd, &hStd_OUT_Wr, &sa, 0)) { _impl.reset(); return; }
    if (!CreatePipe(&hStd_IN_Rd, &_impl->hStd_IN_Wr, &sa, 0)) {
        CloseHandle(_impl->hStd_OUT_Rd); CloseHandle(hStd_OUT_Wr); _impl.reset(); return;
    }

    SetHandleInformation(_impl->hStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(_impl->hStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = {};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStd_OUT_Wr;
    si.hStdInput = hStd_IN_Rd;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "\"%s\"", exe_path.c_str());

    BOOL ok = CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &_impl->pi);
    CloseHandle(hStd_OUT_Wr); CloseHandle(hStd_IN_Rd);

    if (!ok) { _impl.reset(); return; }

    _impl->available = true;
    _impl->send_cmd("uci");
    std::string resp = _impl->read_until("uciok", 5000);
    if (resp.empty()) { _impl->available = false; }
}

StockfishAdapter::~StockfishAdapter() = default;
StockfishAdapter::StockfishAdapter(StockfishAdapter&&) noexcept = default;
StockfishAdapter& StockfishAdapter::operator=(StockfishAdapter&&) noexcept = default;

bool StockfishAdapter::is_available() const { return _impl && _impl->available; }
void StockfishAdapter::send(const std::string& cmd) { if (_impl) _impl->send_cmd(cmd); }
std::string StockfishAdapter::read_until(const std::string& token, int timeout_ms) {
    return _impl ? _impl->read_until(token, timeout_ms) : "";
}

static StockfishAdapter::AnalysisResult parse_analysis(const std::string& response) {
    StockfishAdapter::AnalysisResult r;
    std::istringstream iss(response);
    std::string line;
    int best_depth = 0;
    while (std::getline(iss, line)) {
        if (line.find("info depth") == std::string::npos) continue;
        // Bound lines (fail-high/low) are not exact scores: taking them as
        // truth poisons comparisons (e.g. a transient -728 kept over the
        // exact verdict). Only exact lines count.
        if (line.find("lowerbound") != std::string::npos ||
            line.find("upperbound") != std::string::npos) continue;
        if (line.find("seldepth") == std::string::npos &&
            line.find("depth " + std::to_string(best_depth > 0 ? best_depth : 999)) == std::string::npos) {
            int d = parse_int_after(line, "depth ");
            if (d <= best_depth) continue;
        }
        int d = parse_int_after(line, "depth ");
        if (d < best_depth) continue;
        if (line.find("score mate") != std::string::npos) {
            int mate = parse_int_after(line, "score mate ");
            r.eval_cp = (mate > 0) ? 30000 : -30000;
            r.is_mate = true;
            r.mate_in = mate;
        } else if (line.find("score cp") != std::string::npos) {
            r.eval_cp = parse_int_after(line, "score cp ");
            r.is_mate = false;
        } else continue;
        best_depth = d;
        r.depth = d;
    }
    size_t bm_pos = response.rfind("bestmove ");
    if (bm_pos != std::string::npos) {
        bm_pos += 9;
        size_t bm_end = response.find(' ', bm_pos);
        if (bm_end == std::string::npos) bm_end = response.size();
        r.best_move = response.substr(bm_pos, bm_end - bm_pos);
    }
    return r;
}

StockfishAdapter::AnalysisResult StockfishAdapter::analyze(const std::string& fen, int depth) {
    if (!is_available()) return {};
    _impl->send_cmd("ucinewgame");
    _impl->send_cmd("position fen " + fen);
    _impl->send_cmd("go depth " + std::to_string(depth));
    std::string response = _impl->read_until("bestmove", 30000);
    auto r = parse_analysis(response);
    // Flip eval for black to move (Stockfish returns from side-to-move perspective)
    if (fen.find(" b ") != std::string::npos) {
        r.eval_cp = -r.eval_cp;
        if (r.is_mate) r.mate_in = -r.mate_in;
    }
    return r;
}

StockfishAdapter::AnalysisResult StockfishAdapter::analyze_with_move(
    const std::string& fen, const std::string& move_uci, int depth) {
    if (!is_available()) return {};
    _impl->send_cmd("ucinewgame");
    _impl->send_cmd("position fen " + fen + " moves " + move_uci);
    _impl->send_cmd("go depth " + std::to_string(depth));
    std::string response = _impl->read_until("bestmove", 30000);
    auto r = parse_analysis(response);
    // After making the move, the side flips. Stockfish eval is from new side-to-move.
    // If original position was white to move and we made a white move, now it's black's turn
    // So flip: the eval is from opponent's perspective.
    bool original_white = (fen.find(" w ") != std::string::npos);
    r.eval_cp = -r.eval_cp;
    if (r.is_mate) r.mate_in = -r.mate_in;
    return r;
}

#else

struct StockfishImpl {};
StockfishAdapter::StockfishAdapter() : _impl(nullptr) {}
StockfishAdapter::StockfishAdapter(const std::string&) : _impl(nullptr) {}
StockfishAdapter::~StockfishAdapter() = default;
StockfishAdapter::StockfishAdapter(StockfishAdapter&&) noexcept = default;
StockfishAdapter& StockfishAdapter::operator=(StockfishAdapter&&) noexcept = default;
bool StockfishAdapter::is_available() const { return false; }
void StockfishAdapter::send(const std::string&) {}
std::string StockfishAdapter::read_until(const std::string&, int) { return ""; }
StockfishAdapter::AnalysisResult StockfishAdapter::analyze(const std::string&, int) { return {}; }
StockfishAdapter::AnalysisResult StockfishAdapter::analyze_with_move(const std::string&, const std::string&, int) { return {}; }

#endif
