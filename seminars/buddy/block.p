set terminal png
set output "barplot.png"
set boxwidth 0.5
set style fill solid
plot 'block.dat' using 1:3:xtic(2) with boxes