/**
* This program stresses method calls, memory sustainability
* and looping.
*/

var g_totaltrees = 0;

function Tree(value, depth)
{
    var my = {};
    g_totaltrees++;
    my.depth = depth;
    my.value = value;
    if(depth > 0)
    {
        my.left  = new Tree(value - 1, depth - 1);
        my.right = new Tree(2 * value + 1, depth - 1);
    }
    else
    {
        my.left  = null;
        my.right = null;
    }

	my.check = function()
    {
        if(!my.left)
        {
            return my.value;
        }
        return my.value + my.right.check() - my.left.check();
    }
    return my;
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
