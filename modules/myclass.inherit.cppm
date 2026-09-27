export module myclass:inherit;
import std;

export class animal {
public:
  animal();
  animal(std::string name, std::string sex, int age);
  ~animal();
  std::string _name;
  std::string _sex;
  int _age;

public:
  void bark();
  virtual void who();
  void get_info();
};

export class dog : public animal {
public:
  dog(std::string name, std::string sex, int age);
  ~dog();
  virtual void who();
  void get_info();
};

export class cat : public animal {
public:
  cat(std::string name, std::string sex, int age);
  ~cat();
  virtual void who();
  void get_info();
};