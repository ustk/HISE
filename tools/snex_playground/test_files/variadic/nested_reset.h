/*
BEGIN_TEST_DATA
  f: main
  ret: int
  args: int
  input: 12
  output: 2
  error: ""
  filename: "variadic/nested_reset"
END_TEST_DATA
*/

struct X
{
    int v = 90;
    
    void reset()
    {
        v = 1;
    }
};

chain<chain<X, X>, X, X> c;

int main(int input)
{
	c.reset();
	
	return c.get<1>().v + c.get<2>().v;
}

