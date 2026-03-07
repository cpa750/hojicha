int main(void) {
  long long ret = 0;
  for (int i = 0; i < 1000000000; ++i) { ret += i; }
  return ret;  // Big maths
}

