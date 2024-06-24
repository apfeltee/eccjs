


function calc_point(cx, cy, maxit)
{
    var i = 0
    var xp = 0
    var yp = 0
    while (i < maxit)
    {
        var t = (((xp * xp) - (yp * yp)) + cx)
        yp = (((2 * xp) * yp) + cy)
        xp = t
        if(((xp * xp) + (yp * yp)) > 4)
        {
            break
        }
        i = i + 1
    }
    return i
}

function mandelbrot(x1,y1, x2,y2, size_x,size_y, max_iter)
{
    var step_x = (x2-x1)/(size_x-1)
    var step_y = (y2-y1)/(size_y-1)
    var y = y1
    var ec = String.fromCharCode(27)
    while (y <= y2)
    {
        var x = x1
        while (x <= x2)
        {
            var c = calc_point(x, y, max_iter)
            if (c == max_iter)
            {
                print(ec, "[37;40m ")
            }
            else
            {
                var bc = ((c % 6) + 1)
                print(ec, "[37;4", bc, "m ")
            }
            x = x + step_x
        }
        y = y + step_y
        print(ec, "[0m\n");
    }
}

function get_term_lines()
{
    return 40
}

function doit()
{
    var lines = get_term_lines()
    if (lines <= 0)
    {
        lines = 25
    }
    lines = lines - 1
    if ((lines % 2) == 0)
    {
        lines = lines - 1
    }
    var cols = (2.2 * lines)
    print("lines=", lines, ", cols=", cols, "\n")
    mandelbrot(-2, -2, 2, 2, cols, lines, 100)
}

doit()