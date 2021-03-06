/*
BEGIN_TEST_DATA
  f: main
  ret: int
  args: int
  input: 12
  output: 16
  error: ""
  filename: "span/loop_break_continue_nested2"
END_TEST_DATA
*/

span<span<float, 3>, 5> d = { {1.0f, 2.0f, 3.0f}};

int main(int input)
{
    float sum = 0.0f;
    
    int counter = 0;
    
	for(auto& i: d)
    {
        if(counter++ == 4)
            break;
        
        for(auto& s: i)
        {
            if(s == 2.0f)
                continue;
            
            sum += s;
        }
    }
    
    return (int)sum;
}

