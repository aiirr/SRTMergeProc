// author:  air fu
// date:    2021-1-14
// License: MIT 

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cctype>
#include <stdio.h>

using namespace std;
namespace fs = std::filesystem;

bool Compare(std::string& str1, std::string& str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), [](char& c1, char& c2) {
		return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
		}));
}

bool IsExistedSrt(const fs::path& P);
bool NameIsSrt(const fs::path& P);
bool PathExist(const fs::path& P);

bool StringToMS(const string& Stamp, unsigned int* s, unsigned int* e)
{
    unsigned int a1, a2, a3, a4, b1, b2, b3, b4;
    int ret = sscanf_s(Stamp.c_str(), "%u:%u:%u,%u --> %u:%u:%u,%u", &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4);
    *s = a1 * 3600 * 1000 + a2 * 60 * 1000 + a3 * 1000 + a4;
    *e = b1 * 3600 * 1000 + b2 * 60 * 1000 + b3 * 1000 + b4;
    return ret == 8;
}

string MSToString(unsigned int a, unsigned int b)
{
    char buf[64] = {0};
    unsigned int a1, a2, a3, a4, b1, b2, b3, b4;
    a1 = a / 3600000;
    a2 = (a - a1 * 3600000) / 60000;
    a3 = (a - a1 * 3600000 - a2 * 60000) / 1000;
    a4 = a - a1 * 3600000 - a2 * 60000 - a3 * 1000;


    b1 = b / 3600000;
    b2 = (b - b1 * 3600000) / 60000;
    b3 = (b - b1 * 3600000 - b2 * 60000) / 1000;
    b4 = b - b1 * 3600000 - b2 * 60000 - b3 * 1000;
    sprintf_s(buf, 64, "%02u:%02u:%02u,%03u --> %02u:%02u:%02u,%03u", a1, a2, a3, a4, b1, b2, b3, b4);
    return buf;
}

class SItem 
{
public:
    bool IsSenEnd()const;
    friend bool operator >>(ifstream& f, SItem& si);
    friend bool operator <<(ofstream& f, const SItem& oi);


    bool IsGood() const;
private:
    // 判断是否是普通的结尾，例如是句号，问好感叹号等
    bool IsNormalEnd() const;
    // 判断是假的结尾, 例如Mr.这类的
    bool IsFakeEnd() const;
    // 两句时间非常接近，判定为连着的句子
    bool IsVeryClose() const;

private:
    const unsigned int VeryClose = 50;  // 假设50ms就算非常接近
    const string SenEnd = { ".!?" };

private:
    unsigned int idx = 0;           // 编号
    unsigned int StampStart = 0;    // 一句开始的时间, 单位是毫秒，从片头也就0开始的时间偏移量
    unsigned int StampEnd = 0;      // 一句结尾的时间
    string Content;             // 句子的内容

    bool good = true;
    bool Empty = true;
public:

    void Update(const SItem& s, unsigned int i);
    void Clean() { good = true; Empty = true; }
};

bool operator >>(ifstream& f, SItem& si)
{
    string lines[4];
    for (int i = 0; i < 4 && f.good(); ++i)
    {
        getline(f, lines[i]);
    }
    si.good = (1 == sscanf_s(lines[0].c_str(), "%u", &si.idx));
    si.good = StringToMS(lines[1], &si.StampStart, &si.StampEnd);

    si.Content = lines[2];
    si.Empty = false;
    return si.good;
}

bool operator <<(ofstream& f, const SItem& oi)
{
    string lines[4];
    lines[0] = std::to_string(oi.idx);
    lines[1] = MSToString(oi.StampStart, oi.StampEnd);
    lines[2] = oi.Content;
    lines[3] = "\0";
    for (int i = 0; i < 4; ++i)
    {
        f << lines[i] << endl;
    }
    return true;
}

bool SItem::IsSenEnd() const
{
    return IsNormalEnd() && !(IsFakeEnd()) && !(IsVeryClose());
}

bool SItem::IsGood() const
{
    return good;
}

bool SItem::IsNormalEnd() const
{
    if (Content.length() == 0)
    {
        return true;
    }
    char last = *Content.rbegin();
    auto it = std::find(SenEnd.begin(), SenEnd.end(), last);
    return it != SenEnd.end();
}

bool SItem::IsFakeEnd() const
{
    // 先不考虑
    return false;
}

bool SItem::IsVeryClose() const
{
    // 先不考虑
    return false;
}

void SItem::Update(const SItem& s, unsigned int i)
{
    if (this->Empty)
    {
        this->idx = i;
        this->StampStart = s.StampStart;
        this->StampEnd = s.StampEnd;
        this->Content = s.Content;
        Empty = false;
    }
    else
    {
        this->StampEnd = s.StampEnd;
        this->Content += s.Content;
    }
}

class SManager 
{
public:
    ~SManager();
    bool Init(const fs::path& ISrt, const fs::path& OSrt);
    void Proc();
private:
private:
    ifstream InSrt;
    ofstream OutSrt;

    vector<SItem> InItem;
    vector<SItem> OItem;
};

SManager::~SManager()
{
    InSrt.close();
    OutSrt.close();
}

bool SManager::Init(const fs::path& ISrt, const fs::path& OSrt)
{
    if (!IsExistedSrt(ISrt))
    {
        cerr << ISrt.string() << endl;
        return false;
    }

    if (!(PathExist(OSrt) && NameIsSrt(OSrt)))
    {
        cerr << OSrt.string() << endl;
        return false;
    }
    InSrt.open(ISrt);
    OutSrt.open(OSrt, ios_base::out | ios_base::app);
    return true;
}

void SManager::Proc()
{
    SItem si;
    while (!InSrt.eof())
    {
        InSrt >> si;
        if (si.IsGood())
        {
            InItem.push_back(si);
        }
    }
    SItem so;
    unsigned int idx = 1;
    for (const SItem& i : InItem)
    {
        so.Update(i, idx);
        if (i.IsSenEnd())
        {
            OutSrt << so;
            ++idx;
            so.Clean();
        }
    }
}

bool PathExist(const fs::path& P)
{
    fs::path parent = P.parent_path();
    bool p_exist = fs::exists(parent);
    return p_exist;
}

bool NameIsSrt(const fs::path& P)
{
    string SRT = ".srt";
    string ext = P.extension().string();
    bool has_f = P.has_filename();
    bool has_e = P.has_extension();
    bool is_srt = Compare(ext, SRT);
    return has_f && has_e && is_srt;
}

bool IsExistedSrt(const fs::path& P)
{
    return fs::exists(P) && NameIsSrt(P);
}

int main(int argc, char *argv[])
{
    vector<fs::path> Paths;
    for (int i = 1; i < argc; ++i)
    {
        fs::path ArgPath(argv[i]);
        Paths.push_back(ArgPath);
    }
    if (Paths.size() < 2)
    {
        cout << "path error.";
        return 1;
    }
    SManager Manager;
    Manager.Init(Paths[0], Paths[1]);
    Manager.Proc();
    return 0;
}