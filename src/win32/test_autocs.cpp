#include <iostream>
#include <cassert>
#include <string>
#include "config.h"
#include "autocs.h"
#include "../ukinterface/unikey.h"
#include "../vnconv/vnconv.h"

void TestConfigManager() {
    std::cout << "[TEST] Running ConfigManager tests..." << std::endl;
    ConfigManager& cfg = ConfigManager::Instance();
    cfg.Init(L"dist\\app_rules.ini");

    // Test default mappings
    int chromeCs = cfg.GetCharsetForProcess("chrome.exe");
    assert(chromeCs == CONV_CHARSET_XUTF8);
    std::cout << "  - chrome.exe -> Unicode (ID: " << chromeCs << ") PASS" << std::endl;

    int zaloCs = cfg.GetCharsetForProcess("zalo.exe");
    assert(zaloCs == CONV_CHARSET_XUTF8);
    std::cout << "  - zalo.exe -> Unicode (ID: " << zaloCs << ") PASS" << std::endl;

    int sCs = cfg.GetCharsetForProcess("s.exe");
    assert(sCs == CONV_CHARSET_TCVN3);
    std::cout << "  - s.exe -> TCVN3 (ID: " << sCs << ") PASS" << std::endl;

    // Test case-insensitivity
    int chromeUpper = cfg.GetCharsetForProcess("CHROME.EXE");
    assert(chromeUpper == CONV_CHARSET_XUTF8);
    std::cout << "  - CHROME.EXE -> Unicode PASS" << std::endl;

    int sUpper = cfg.GetCharsetForProcess("S.Exe");
    assert(sUpper == CONV_CHARSET_TCVN3);
    std::cout << "  - S.Exe -> TCVN3 PASS" << std::endl;

    // Test unmapped process fallback to Unicode
    int unknownCs = cfg.GetCharsetForProcess("notepad.exe");
    assert(unknownCs == CONV_CHARSET_XUTF8);
    std::cout << "  - notepad.exe (unmapped) -> default Unicode PASS" << std::endl;

    // Test add and remove rule
    cfg.SetRule("word.exe", "VNI");
    int wordCs = cfg.GetCharsetForProcess("word.exe");
    assert(wordCs == CONV_CHARSET_VNIWIN);
    std::cout << "  - Added word.exe -> VNI PASS" << std::endl;

    cfg.RemoveRule("word.exe");
    int wordAfterRemove = cfg.GetCharsetForProcess("word.exe");
    assert(wordAfterRemove == CONV_CHARSET_XUTF8);
    std::cout << "  - Removed word.exe -> default Unicode PASS" << std::endl;

    std::cout << "[PASS] ConfigManager tests passed successfully!" << std::endl;
}

void TestUniKeyEngineIntegration() {
    std::cout << "[TEST] Running UniKey Engine Integration tests..." << std::endl;

    UnikeySetup();
    UnikeySetInputMethod(UkTelex);

    // 1. Unicode Test: 'a' + 's' -> 'à'
    UnikeySetOutputCharset(CONV_CHARSET_XUTF8);
    UnikeyResetBuf();
    UnikeySetCapsState(0, 0);

    UnikeyFilter('a');
    // First letter 'a': no transform yet
    assert(UnikeyBackspaces == 0);

    UnikeyFilter('s');
    // Second letter 's' triggers Telex tone huyền: deletes 'a' (1 backspace) and outputs 'à'
    assert(UnikeyBackspaces == 1);
    assert(UnikeyBufChars > 0);
    std::cout << "  UnikeyBufChars: " << UnikeyBufChars << " bytes: ";
    for (int i = 0; i < UnikeyBufChars; i++) {
        std::cout << "0x" << std::hex << (int)(unsigned char)UnikeyBuf[i] << " ";
    }
    std::cout << std::dec << std::endl;
    // UTF-8 encoding of 'á' (Telex 'a' + 's') is 0xC3 0xA1
    assert(UnikeyBufChars == 2);
    assert((unsigned char)UnikeyBuf[0] == 0xC3 && (unsigned char)UnikeyBuf[1] == 0xA1);
    std::cout << "  - Telex typing in Unicode: 'a' + 's' -> 'á' (UTF-8: 0xC3 0xA1) PASS" << std::endl;

    // 2. TCVN3 Test: 'a' + 's' -> 'á' (TCVN3 single-byte 0xB8)
    UnikeySetOutputCharset(CONV_CHARSET_TCVN3);
    UnikeyResetBuf();
    UnikeySetCapsState(0, 0);

    UnikeyFilter('a');
    assert(UnikeyBackspaces == 0);

    UnikeyFilter('s');
    assert(UnikeyBackspaces == 1);
    assert(UnikeyBufChars == 1);
    // TCVN3 encoding of 'á' is 0xB8
    assert((unsigned char)UnikeyBuf[0] == 0xB8);
    std::cout << "  - Telex typing in TCVN3: 'a' + 's' -> 'á' (TCVN3 byte 0xB8) PASS" << std::endl;

    // 3. VNI-Windows Test: 'a' + 's' -> 'á'
    UnikeySetOutputCharset(CONV_CHARSET_VNIWIN);
    UnikeyResetBuf();
    UnikeySetCapsState(0, 0);

    UnikeyFilter('a');
    UnikeyFilter('s');
    assert(UnikeyBackspaces == 1);
    assert(UnikeyBufChars > 0);
    std::cout << "  - Telex typing in VNI-Windows: 'a' + 's' -> 'á' PASS" << std::endl;

    UnikeyCleanup();
    std::cout << "[PASS] UniKey Engine Integration tests passed successfully!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  UniKey Auto-Charset Automated Tests  " << std::endl;
    std::cout << "========================================" << std::endl;

    TestConfigManager();
    TestUniKeyEngineIntegration();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL TESTS PASSED WITH 100% SUCCESS!   " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
