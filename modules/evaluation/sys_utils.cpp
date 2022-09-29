
#include <fstream>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include "sys_utils.hpp"
using namespace std;
// const char * get_framelist_path(const char *file_path){
//     string path;
//     path = file_path;
//     string list_path = path+"/framelist.txt";
//     return list_path.c_str();
// }
// const char * get_frame_path(const char *file_path, const char *image_path){
//     string path;
//     string image;
//     image = image_path;
//     path = file_path;
//     image.erase(image.begin());
//     string frame_path = path + image;
//     return frame_path.c_str();
// }
// const char * get_out_path(const char *out_path, const char *image_path){
//     string path;
//     string image;
//     image = image_path;
//     path = out_path;
//     image.erase(image.begin());
//     image.erase(image.end()-4,image.end());
//     string out_path = path + image + "txt";
//     return out_path.c_str();
// }

std::vector<std::string> read_file_lines(std::string strfile) {
  std::fstream file(strfile);
  std::vector<std::string> lines;
  if (!file.is_open()) {
    return lines;
  }
  std::string line;
  while (getline(file, line)) {
    if (line.length() > 0 && line[0] == '#') continue;
    lines.push_back(line);
  }
  return lines;
}
std::string replace_file_ext(const std::string &src_file_name, const std::string &new_file_ext) {
  size_t ext_pos = src_file_name.find_last_of('.');
  size_t seg_pos = src_file_name.find_first_of('/');
  std::string src_name = src_file_name;
  if (seg_pos != src_file_name.npos) {
    src_name.at(seg_pos) = '#';
  }
  std::string str_res;
  if (ext_pos == src_file_name.npos) {
    str_res = src_name + std::string(".") + new_file_ext;
  } else {
    str_res = src_name.substr(0, ext_pos) + std::string(".") + new_file_ext;
  }
  return str_res;
}

bool create_directory(const std::string &str_dir) {
  if (mkdir(str_dir.c_str(), 0777) == -1) return false;
  return true;
}
std::string join_path(const std::string &str_path_parent, const std::string &str_path_sub) {
  std::string str_res = str_path_parent;
  if (str_res.at(str_res.size() - 1) != '/') {
    str_res = str_res + std::string("/");
  }
  str_res = str_res + str_path_sub;
  return str_res;
}
std::string get_directory_name(const std::string &str_path) {
  std::string strname = str_path;
  size_t seg_pos = strname.find_last_of('/');
  if (seg_pos == strname.length() - 1) {  // if '/' is the last pos,drop it then split again
    strname = strname.substr(0, seg_pos);
    seg_pos = strname.find_last_of('/');
  }
  if (seg_pos != strname.npos) {
    strname = strname.substr(seg_pos + 1, strname.length() - 1 - seg_pos);
  }
  return strname;
}