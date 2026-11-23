#include "checker.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

// 2장 A 튜토리얼용 checker_scan C-Simulation 테스트벤치
extern "C" void checker_scan(
    const uint16_t* raw_bayer, int width, int height, int mode,
    uint16_t dark_pixel_threshold, int verdict_pct, int enter_pct, int exit_pct,
    int* selected_mode, int* hyst_flags, int* dark_count);

namespace {
constexpr int kWidth = 100;
constexpr int kHeight = 10;
constexpr uint16_t kThreshold = 256;  // 보드 실운용값: raw12 16<<4
constexpr int kVerdict = 62;
constexpr int kEnter = 64;
constexpr int kExit = 60;

struct Expected {
    int mode;
    int flags;
    int dark;
};

// checker_select_mode를 호출하지 않고 정수 산술로 독립 기대값 계산
Expected compute_expected(const uint16_t* raw, int w, int h, int mode) {
    if (!raw || w <= 0 || h <= 0 || mode == DFXISP_MODE_NORMAL)
        return {DFXISP_MODE_NORMAL, DFXISP_HYST_BELOW_EXIT, 0};
    if (mode == DFXISP_MODE_LOW_LIGHT)
        return {DFXISP_MODE_LOW_LIGHT, DFXISP_HYST_ABOVE_ENTER, 0};

    const int n = w * h;
    int dark = 0;
    for (int i = 0; i < n; ++i) {
        if (raw[i] < kThreshold) ++dark;
    }
    const int scaled = dark * 100;
    const int sel = (scaled > kVerdict * n) ? DFXISP_MODE_LOW_LIGHT : DFXISP_MODE_NORMAL;
    const int flg = ((scaled > kEnter * n) ? DFXISP_HYST_ABOVE_ENTER : 0) |
                    ((scaled < kExit * n) ? DFXISP_HYST_BELOW_EXIT : 0);
    return {sel, flg, dark};
}

bool run_case(const std::string& name, const uint16_t* raw, int w, int h, int mode) {
    const Expected exp = compute_expected(raw, w, h, mode);
    int act_mode = -1, act_flags = -1, act_dark = -1;
    checker_scan(raw, w, h, mode, kThreshold, kVerdict, kEnter, kExit,
                 &act_mode, &act_flags, &act_dark);
    if (act_mode == exp.mode && act_flags == exp.flags && act_dark == exp.dark) return true;
    std::cerr << "FAIL " << name
              << " expected=(" << exp.mode << "," << exp.flags << "," << exp.dark << ")"
              << " actual=(" << act_mode << "," << act_flags << "," << act_dark << ")\n";
    return false;
}
}  // namespace

int main() {
    std::array<uint16_t, 2048> raw{};
    int passed = 0, total = 0;
    auto check = [&](const std::string& name, const uint16_t* ptr, int w, int h, int mode) {
        ++total;
        if (run_case(name, ptr, w, h, mode)) ++passed;
    };

    // AUTO 모드 경계 검증 (raw == 256은 어둡지 않음, 255만 어두움)
    for (int dark : {0, 599, 600, 601, 619, 620, 621, 639, 640, 641, 1000}) {
        raw.fill(kThreshold);
        std::fill_n(raw.begin(), dark, static_cast<uint16_t>(kThreshold - 1));
        check("auto_dark_" + std::to_string(dark), raw.data(), kWidth, kHeight, DFXISP_MODE_AUTO);
    }

    // 강제 모드 검증
    check("forced_normal", raw.data(), kWidth, kHeight, DFXISP_MODE_NORMAL);
    raw.fill(static_cast<uint16_t>(kThreshold + 1));
    check("forced_lowlight", raw.data(), kWidth, kHeight, DFXISP_MODE_LOW_LIGHT);

    // 입력 가드 검증
    check("guard_zero_w", raw.data(), 0, kHeight, DFXISP_MODE_AUTO);
    check("guard_zero_h", raw.data(), kWidth, 0, DFXISP_MODE_AUTO);
    check("guard_null", nullptr, kWidth, kHeight, DFXISP_MODE_LOW_LIGHT);

    std::cout << (passed == total ? "PASS" : "FAIL")
              << " cases=" << passed << "/" << total << "\n";
    return (passed == total) ? 0 : 1;
}
