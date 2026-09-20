export module mystl:io;
import std;

export namespace io{
  template <typename T>
  void ls_println(T&& ls){
    for(auto&& i:ls){
       std::print("{} ",i);
    }
    std::println();
  }
  void test(int a);

}