export module myclass;
import std;

export class dog {
public:
  dog();
  dog(std::string name, std::string sex, int age);
  ~dog();
  std::string _name;
  std::string _sex;
  int _age;

public:
  void bark();
  void get_info();
};
