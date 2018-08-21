#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

// split string by space
vector<string> split(const string& s, const std::string& c)
{
    vector<string> v;
    string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(string::npos != pos2)
    {
        v.push_back(s.substr(pos1, pos2-pos1));

        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if(pos1 != s.length())
        v.push_back(s.substr(pos1));
    return v;
}

int main() {
    const char *testfile = "/home/liwei/Workspace/Projects/wear-leveling/tools/wearcount/output.txt";
    int max = 0;
    int i, j, tmp_i, tmp_j; // max wearcount index
    ifstream infile(testfile);
    string line;
    tmp_i = 0;
    while (getline(infile, line)) {
        tmp_i++; 
        tmp_j = 0;
        vector<string> splits = split(line, " ");
        for (auto str : splits) {
            tmp_j++;
            int wearcount = atoi(str.c_str());
            if (wearcount > max) {
                max = wearcount;
                i = tmp_i; 
                j = tmp_j;
            }
        }
    }
    cout << "max wearcount : " << max << ", index: (" << i << "," << j << ")" << endl;
}