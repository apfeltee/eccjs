
var g_callmain = 0;
var g_callsub = 0;
function A(k, plusone, minusone, minusanother, plusanother, plusnull)
{
    g_callmain++;
    function B()
    {
        g_callsub++;
        k = k - 1;
        return A(k, B, plusone, minusone, minusanother, plusanother);
    }
    if (k <= 0)
    {
        return plusanother() + plusnull();
    }
    return B();
}

function vnull()  { return  0; }
function vone()  { return  1; }
function vminusone() { return -1; }

for(var i=0; i<12; i++)
{
    g_callmain = 0;
    g_callsub = 0;
    print("", i, " -> ");
    try
    {
        var res = A(i, vone, vminusone, vminusone, vone, vnull);
        println(res, " (with ", g_callmain, " mains calls, and ", g_callsub, " sub calls)");
    }
    catch(e)
    {
        println("error: ", e.message, " (from ", e.srcfile, ":", e.srcline, ")");
        break;
    }
}
