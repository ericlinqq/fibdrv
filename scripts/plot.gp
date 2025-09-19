reset
set xlabel 'F(n)'
set ylabel 'time (ns)'
set title 'Fibonacci runtime'
set term png
set output 'plot.png'
set grid
plot [0:92][0:600]'plot_input'\
using 1:2 with linespoints linewidth 2 title 'naive',\
'' using 1:3 with linespoints linewidth 2 title 'fast doubling',\
'' using 1:4 with linespoints linewidth 2 title 'fast doubling w/ clz'
