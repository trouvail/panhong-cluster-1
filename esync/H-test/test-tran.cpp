#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "用法: " << argv[0] << " <源文件路径> <目标文件路径>" << endl;
        return 1;
    }

    string sourceFilePath = argv[1];
    string targetFilePath = argv[2];

    // 打开源文件
    ifstream sourceFile(sourceFilePath);
    if (!sourceFile.is_open())
    {
        cerr << "无法打开源文件：" << sourceFilePath << endl;
        return 1;
    }

    // 打开目标文件，如果不存在则创建
    ofstream targetFile(targetFilePath);
    if (!targetFile.is_open())
    {
        cerr << "无法创建或打开目标文件：" << targetFilePath << endl;
        return 1;
    }

    // 读取源文件
    string sourceContent((istreambuf_iterator<char>(sourceFile)),
                         istreambuf_iterator<char>());

    cout << sourceContent << endl;

    // 逐行复制源文件内容到目标文件
    string line;
    while (getline(sourceFile, line))
    {
        targetFile << line << endl;
    }

    // 关闭文件流
    sourceFile.close();
    targetFile.close();

    cout << "文件传输完成！" << endl;

    return 0;
}

// 检测差异 ------------------------------ start
// #include <iostream>
// #include <fstream>
// #include <vector>
// #include <algorithm>

// using namespace std;

// // 检测两个字符串的差异，并返回差异的位置
// vector<int> findDifference(const string& str1, const string& str2) {
//     vector<int> differencePositions;

//     for (int i = 0; i < min(str1.length(), str2.length()); ++i) {
//         if (str1[i] != str2[i]) {
//             differencePositions.push_back(i);
//         }
//     }

//     return differencePositions;
// }

// // 从源文件传输差异到目标文件
// void transferDifference(const string& sourceFilePath, const string& targetFilePath, const vector<int>& differencePositions) {
//     // 打开源文件
//     ifstream sourceFile(sourceFilePath);
//     if (!sourceFile.is_open()) {
//         cerr << "无法打开源文件：" << sourceFilePath << endl;
//         return;
//     }

//     // 打开目标文件，使用 ios::app 使得写入内容附加到文件末尾
//     ofstream targetFile(targetFilePath, ios::app);
//     if (!targetFile.is_open()) {
//         cerr << "无法创建或打开目标文件：" << targetFilePath << endl;
//         return;
//     }

//     // 移动文件流到目标文件的末尾
//     targetFile.seekp(0, ios::end);

//     // 读取源文件的内容
//     string sourceContent((istreambuf_iterator<char>(sourceFile)),
//                                istreambuf_iterator<char>());

//     // 写入源文件中差异的部分到目标文件
//     for (int pos : differencePositions) {
//         targetFile << sourceContent[pos];
//     }

//     // 关闭文件流
//     sourceFile.close();
//     targetFile.close();

//     cout << "文件传输完成！" << endl;
// }

// int main(int argc, char* argv[]) {
//     if (argc != 3) {
//         cerr << "用法: " << argv[0] << " <源文件路径> <目标文件路径>" << endl;
//         return 1;
//     }

//     string sourceFilePath = argv[1];
//     string targetFilePath = argv[2];

//     // 打开目标文件
//     ifstream targetFile(targetFilePath);
//     if (!targetFile.is_open()) {
//         cerr << "无法打开目标文件：" << targetFilePath << endl;
//         return 1;
//     }

//     // 读取目标文件的内容
//     string targetContent((istreambuf_iterator<char>(targetFile)),
//                                istreambuf_iterator<char>());

//     // 关闭目标文件流
//     targetFile.close();

//     // 执行差异检测
//     string sourceContent;
//     vector<int> differencePositions;

//     if (!targetContent.empty()) {
//         // 目标文件非空，进行差异检测
//         ifstream sourceFile(sourceFilePath);
//         if (!sourceFile.is_open()) {
//             cerr << "无法打开源文件：" << sourceFilePath << endl;
//             return 1;
//         }

//         sourceContent = string((istreambuf_iterator<char>(sourceFile)),
//                                     istreambuf_iterator<char>());

//         differencePositions = findDifference(sourceContent, targetContent);
//         sourceFile.close();
//     } else {
//         // 目标文件为空，直接使用源文件内容
//         ifstream sourceFile(sourceFilePath);
//         if (!sourceFile.is_open()) {
//             cerr << "无法打开源文件：" << sourceFilePath << endl;
//             return 1;
//         }

//         sourceContent = string((istreambuf_iterator<char>(sourceFile)),
//                                     istreambuf_iterator<char>());

//         sourceFile.close();
//     }

//     // 执行文件传输
//     transferDifference(sourceFilePath, targetFilePath, differencePositions);

//     return 0;
// }
// 检测差异 ------------------------------ end

// g++ test-tran.cpp -o test-tran
// ./test-tran tsrc tdst