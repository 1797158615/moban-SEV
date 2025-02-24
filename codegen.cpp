#include <iostream>
#include <string>
#include <fstream>  // 添加文件流的头文件
#include "ta_server_gen.h"
#include "td_setup_gen.h"


using namespace std;

int main(int argc, char* argv[])
{
    // 检查是否有输入参数
    if (argc < 3) {
        std::cout << "请提供至少两个参数。" << std::endl;
        return 1;
    }
    
    string path = "./ta_server.c";
    string comm = argv[2];
    if(argc > 3) {
    	for(int i = 3; i < argc; i++){
    		comm += " ";
    		comm += argv[i];
    	}
    }

    // 创建 宿主机对象
    ta_server_gen_class ta_server(comm);
    
    // 使用 ofstream 进行文件操作
    ofstream ofs(path);  // 打开文件
    if (!ofs) {  // 检查文件是否成功打开
        cerr << "无法打开文件: " << path << endl;
        return 1;
    }
    
    // 调用生成函数，修改为接受 ofstream
    ta_server.generate_ta_server(ofs);  // 需要确保该函数接受 ofstream 类型
    ofs.close();  // 关闭文件流
    
    //生成shell
    path = "./ta_setup";
    
    td_setup_gen_class td_setup(argv[1]);
    ofstream td(path);
    if (!td) {  // 检查文件是否成功打开
        cerr << "无法打开文件: " << path << endl;
        return 1;
    }
    td_setup.generate_td_setup(td);
    td.close();
    
    
    
    
    return 0;
}
