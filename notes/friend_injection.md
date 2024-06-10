Fear not, there are other more clever solutions to this. 

Since as we all know "_clever_" is just an euphemism for "_abusing obscure language features that probably shouldn't be a thing_" let's take a moment to talk about everyone's second favorite language feature ADL and friend injection.

> Should you? Probably not.
> > **2118. Stateful metaprogramming via friend injection**
> >
> > Defining a friend function in a template, then referencing that function later provides a means of capturing and retrieving metaprogramming state. This technique is arcane and should be made ill-formed.
> >
> > **Notes from the May, 2015 meeting:**
> >
> > CWG agreed that such techniques should be ill-formed, although the mechanism for prohibiting them is as yet undetermined.
> 
> [C++ Standard Core Language Active Issues](https://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#2118)
{: .prompt-warning }

(Pro Tip: The [C++ Standard Core Language Active Issues](https://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html) list is a great resource to learn about extremely obscure C++ "features")
