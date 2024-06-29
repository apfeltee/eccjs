
function Tree(item, depth)
{
    var my = {};
    my.item = item;
    my.depth = depth;
    if (depth > 0)
    {
        var item2 = item + item;
        depth = depth - 1;
        my.left = new Tree(item2 - 1, depth);
        my.right = new Tree(item2, depth);
    }
    else
    {
        my.left = null;
        my.right = null;
    }
    my.check = function()
    {
        if(my.left == null)
        {
            return my.item;
        }
        var a = my.item;
        var b = my.left.check();
        var c = my.right.check();
        return a + b - c;
    }
    return my;
}

function clock()
{
    return 0
}

var mindepth = 4;
var maxdepth = mindepth*2;
var stretchdepth = maxdepth + 1;

var start = clock();

var dep =  (new Tree(0, stretchdepth)).check();
print("stretch tree of depth:", stretchdepth, " check:",dep, "\n");

var longlivedtree = new Tree(0, maxdepth);


var iterations = 1;
var d = 0;
while (d < maxdepth)
{
    iterations = iterations * 2;
    d = d + 1;
}

var i = 0;
var checkme = 0;
var depth = mindepth;
while (depth < stretchdepth)
{
    checkme = 0;
    i = 1;
    while (i <= iterations)
    {
        var t1 = (new Tree(i, depth)).check();
        var t2 = (new Tree(-i, depth)).check();
        checkme = checkme + t1 + t2;
        i = i + 1;
    }
    print("num trees:", iterations * 2, ", depth:", depth, ", checkme:", checkme, "\n");
    iterations = iterations / 4;
    depth = depth + 2;
}

print("long lived tree of depth:", maxdepth, ", check:", longlivedtree.check(), ", elapsed:", clock() - start, "\n");
