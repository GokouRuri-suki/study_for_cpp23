import std;
import utils;

int main() {
  std::print("Hello World!\n");

  int size {0};
  std::cin>>size;
  std::vector<int> vec;
  for(int i = 0; i < size; i++){ 
    int num;
    std::cin>>num;
    vec.push_back(num);
  }
  std::print("yes\n");
  list_print(vec);

  return 0;
}