set log y
plot 'res.dat' using 1:3 title 'Allocated' with lines lw 2, \
'res.dat' using 1:4 title 'Total allocated' with lines lw 2 lc rgb "#FF9000"
set terminal png size 800,600
set output "result.png"
replot

