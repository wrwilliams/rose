// Test for hiding class name where global qualification is required.

struct A
   {
     struct B
        {
        };
   };


void foo()
   {
     typedef int A;

  // Type elaboration is not required here, but the global qualification is required (but only for GNU, not for EDG).
     ::A::B x;
   }

