/**
 * This program stresses method calls, memory sustainability
 * and looping.
 *
 * Part of Promit Test Suit, location /test/Benchmark/BinaryTrees.Promit
 * 
 * Created by SD Asif Hossein in 21th June, 2022 (Tuesday) at 01:31 PM (UTC +06).
 */

/*

old:
==17234== HEAP SUMMARY:
==17234==     in use at exit: 0 bytes in 0 blocks
==17234==   total heap usage: 102,004 allocs, 102,004 frees, 14,546,573 bytes allocated
==17234==
==17234== All heap blocks were freed -- no leaks are possible
==17234==
==17234== For lists of detected and suppressed errors, rerun with: -s
==17234== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

baaaad. very bad.


*/

var g_totaltrees = 0;

class Tree
{
	constructor(value, depth)
    {
        g_totaltrees++;
        this.depth = depth;
        this.value = value;
		if(depth > 0)
        {
            this.left  = new Tree(value - 1, depth - 1);
            this.right = new Tree(2 * value + 1, depth - 1);
        }
        else
        {
            this.left  = null;
            this.right = null;
        }
    }

	check()
    {
        if(!this.left)
        {
            return this.value;
        }
        return this.value + this.right.check() - this.left.check();
    }
}

var mindepth = 4;
var maxdepth = mindepth*2;
var stretchdepth = maxdepth + 1;
print("Min depth : ", mindepth, ", max depth : ", maxdepth, " and stretched depth : ", stretchdepth, "\n");
print("Starting benchmark...\n");
print("Check 1 : ", (new Tree(0, stretchdepth)).check(), "\n");
var ancient = new Tree(0, maxdepth);
var totaltrees = 1;
var i = 0;
while(i < maxdepth) 
{
	totaltrees *= 2;
    i++;
}
var checkcnt = 2;
i = mindepth;
while(i < maxdepth)
{
    var checkval = 0;
    var j = 0;
    while(j < totaltrees)
    {
        checkval += (new Tree(j, i)).check() + (new Tree(-j, i)).check();
        j++;
    }
    print("Number of trees : ", totaltrees * 2, "\n");
    print("Current running depth : ", i, "\n");
    checkcnt++;
    var actualval = (checkval == null) ? "(no value)" : checkval;
    print("Check ", checkcnt, " : ", actualval, "\n");
    totaltrees /= 4;
    i += 2;
}
print("Long lived tree depth : ", maxdepth, "\n");
print("Check ", checkcnt, " : ", ancient.check(), "\n");
print("Benchmarking finished (created ", g_totaltrees, " Trees in total)\n");
