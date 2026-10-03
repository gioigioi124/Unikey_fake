#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

ConfigManager& ConfigManager::Instance() {
    static ConfigManager s_instance;
    return s_instance;
}

ConfigManager::ConfigManager() {
    InitializeCriticalSection(&m_cs);
}

ConfigManager::~ConfigManager() {
    DeleteCriticalSection(&m_cs);
}

std::string ConfigManager::ToLower(const std::string& str) {
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return res;
}

bool ConfigManager::Init(const std::wstring& iniPath) {
    EnterCriticalSection(&m_cs);
    if (!iniPath.empty()) {
        m_iniPath = iniPath;
    } else {
        wchar_t modulePath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, modulePath, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(modulePath, L'\\');
        if (lastSlash) {
            *(lastSlash + 1) = L'\0';
            m_iniPath = std::wstring(modulePath) + L"app_rules.ini";
        } else {
            m_iniPath = L"app_rules.ini";
        }
    }
    LeaveCriticalSection(&m_cs);
    return Load();
}

std::wstring ConfigManager::GetIniPath() const {
    EnterCriticalSection(&m_cs);
    std::wstring path = m_iniPath;
    LeaveCriticalSection(&m_cs);
    return path;
}

bool ConfigManager::Load() {
    EnterCriticalSection(&m_cs);
    m_rules.clear();

    // Check if file exists, if not initialize with default rules
    DWORD attr = GetFileAttributesW(m_iniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        // Defaults specified in R2:
        // zalo.exe, chrome.exe -> Unicode; s.exe -> TCVN3
        m_rules["chrome.exe"] = "Unicode";
        m_rules["zalo.exe"] = "Unicode";
        m_rules["s.exe"] = "TCVN3";
        LeaveCriticalSection(&m_cs);
        Save();
        return true;
    }

    FILE* fp = _wfopen(m_iniPath.c_str(), L"r");
    if (!fp) {
        // Fallback default
        m_rules["chrome.exe"] = "Unicode";
        m_rules["zalo.exe"] = "Unicode";
        m_rules["s.exe"] = "TCVN3";
        LeaveCriticalSection(&m_cs);
        return false;
    }

    char lineBuf[1024];
    bool inRulesSection = false;

    while (fgets(lineBuf, sizeof(lineBuf), fp)) {
        std::string line(lineBuf);
        // Trim leading and trailing whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, (last - first + 1));

        // Skip comments
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[') {
            std::string section = ToLower(line);
            if (section == "[rules]") {
                inRulesSection = true;
            } else {
                inRulesSection = false;
            }
            continue;
        }

        if (inRulesSection) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string proc = line.substr(0, eqPos);
                std::string charset = line.substr(eqPos + 1);

                // Trim proc
                size_t pLast = proc.find_last_not_of(" \t");
                if (pLast != std::string::npos) proc = proc.substr(0, pLast + 1);
                size_t pFirst = proc.find_first_not_of(" \t");
                if (pFirst != std::string::npos) proc = proc.substr(pFirst);

                // Trim charset
                size_t cStart = charset.find_first_not_of(" \t");
                if (cStart != std::string::npos) charset = charset.substr(cStart);
                size_t cLast = charset.find_last_not_of(" \t");
                if (cLast != std::string::npos) charset = charset.substr(0, cLast + 1);

                if (!proc.empty() && !charset.empty()) {
                    m_rules[ToLower(proc)] = charset;
                }
            }
        }
    }
    fclose(fp);

    // Ensure defaults exist if rules map was empty
    if (m_rules.empty()) {
        m_rules["chrome.exe"] = "Unicode";
        m_rules["zalo.exe"] = "Unicode";
        m_rules["s.exe"] = "TCVN3";
    }

    LeaveCriticalSection(&m_cs);
    return true;
}

bool ConfigManager::Save() {
    EnterCriticalSection(&m_cs);
    FILE* fp = _wfopen(m_iniPath.c_str(), L"w");
    if (!fp) {
        LeaveCriticalSection(&m_cs);
        return false;
    }

    fputs("# UniKey Auto-Charset Configuration\n", fp);
    fputs("# Syntax: <process_name.exe> = <CharsetName>\n", fp);
    fputs("# Supported Charsets: Unicode, TCVN3, VNI, VIQR, BK HCM 1, BK HCM 2\n\n", fp);
    fputs("[Rules]\n", fp);

    for (const auto& pair : m_rules) {
        std::string line = pair.first + "=" + pair.second + "\n";
        fputs(line.c_str(), fp);
    }

    fclose(fp);
    LeaveCriticalSection(&m_cs);
    return true;
}

int ConfigManager::GetCharsetForProcess(const std::string& processName) const {
    EnterCriticalSection(&m_cs);
    std::string key = ToLower(processName);
    auto it = m_rules.find(key);
    if (it != m_rules.end()) {
        int id = CharsetNameToId(it->second);
        LeaveCriticalSection(&m_cs);
        return id;
    }
    LeaveCriticalSection(&m_cs);
    // Default to Unicode if not found
    return CONV_CHARSET_XUTF8;
}

std::string ConfigManager::GetCharsetNameForProcess(const std::string& processName) const {
    EnterCriticalSection(&m_cs);
    std::string key = ToLower(processName);
    auto it = m_rules.find(key);
    if (it != m_rules.end()) {
        std::string name = it->second;
        LeaveCriticalSection(&m_cs);
        return name;
    }
    LeaveCriticalSection(&m_cs);
    return "Unicode";
}

void ConfigManager::SetRule(const std::string& processName, const std::string& charsetName) {
    if (processName.empty() || charsetName.empty()) return;
    EnterCriticalSection(&m_cs);
    m_rules[ToLower(processName)] = charsetName;
    LeaveCriticalSection(&m_cs);
}

bool ConfigManager::RemoveRule(const std::string& processName) {
    EnterCriticalSection(&m_cs);
    size_t count = m_rules.erase(ToLower(processName));
    LeaveCriticalSection(&m_cs);
    return count > 0;
}

void ConfigManager::ClearRules() {
    EnterCriticalSection(&m_cs);
    m_rules.clear();
    LeaveCriticalSection(&m_cs);
}

const std::map<std::string, std::string>& ConfigManager::GetAllRules() const {
    return m_rules;
}

int ConfigManager::CharsetNameToId(const std::string& name) {
    std::string lower = ToLower(name);
    if (lower == "unicode" || lower == "utf8" || lower == "utf-8" || lower == "xutf8") {
        return CONV_CHARSET_XUTF8;
    } else if (lower == "tcvn3" || lower == "tcvn" || lower == "abc") {
        return CONV_CHARSET_TCVN3;
    } else if (lower == "vni" || lower == "vni windows" || lower == "vniwin") {
        return CONV_CHARSET_VNIWIN;
    } else if (lower == "viqr") {
        return CONV_CHARSET_VIQR;
    } else if (lower == "bk hcm 1" || lower == "bkhcm1") {
        return CONV_CHARSET_BKHCM1;
    } else if (lower == "bk hcm 2" || lower == "bkhcm2") {
        return CONV_CHARSET_BKHCM2;
    }
    return CONV_CHARSET_XUTF8;
}

std::string ConfigManager::CharsetIdToName(int id) {
    switch (id) {
    case CONV_CHARSET_XUTF8:
    case CONV_CHARSET_UNICODE:
        return "Unicode";
    case CONV_CHARSET_TCVN3:
        return "TCVN3";
    case CONV_CHARSET_VNIWIN:
        return "VNI";
    case CONV_CHARSET_VIQR:
        return "VIQR";
    case CONV_CHARSET_BKHCM1:
        return "BK HCM 1";
    case CONV_CHARSET_BKHCM2:
        return "BK HCM 2";
    default:
        return "Unicode";
    }
}

std::vector<std::string> ConfigManager::GetAvailableCharsets() {
    return { "Unicode", "TCVN3", "VNI", "VIQR", "BK HCM 1", "BK HCM 2" };
}
