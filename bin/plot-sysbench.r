result_vanilla <- scan("sysbench-vanilla/results.txt")

result_6_1 <- scan("sysbench-randomized-6-1/results.txt")
result_6_2 <- scan("sysbench-randomized-6-2/results.txt")
result_6_8 <- scan("sysbench-randomized-6-8/results.txt")
result_6_16 <- scan("sysbench-randomized-6-16/results.txt")

result_12_1 <- scan("sysbench-randomized-12-1/results.txt")
result_12_2 <- scan("sysbench-randomized-12-2/results.txt")
result_12_8 <- scan("sysbench-randomized-12-8/results.txt")
result_12_16 <- scan("sysbench-randomized-12-16/results.txt")

do_graph <- function (name, variables)
{
  postscript (name)
  minimum <- NULL
  maximum <- NULL
  for (variable in variables)
  {
    minimum <- min (get (variable), minimum)
    maximum <- max (get (variable), maximum)
  }
  limits <- c (minimum, maximum)
  plot (result_vanilla, pch = 0, ylim = limits)
  for (variable_index in 1:length (variables))
  {
    variable = variables [variable_index]
    points (get (variable), pch = variable_index, col = variable_index)
  }
  legend ("bottomright", variables, pch = 1:length (variables), col = 1:length (variables))
  dev.off ()
}

do_graph ("results-6.ps", c ("result_6_1", "result_6_2", "result_6_8", "result_6_16"))
do_graph ("results-12.ps", c ("result_12_1", "result_12_2", "result_12_8", "result_12_16"))

postscript ("results-box.ps")
boxplot (list (result_vanilla, result_6_1, result_6_2, result_6_8, result_6_16, result_12_1, result_12_2, result_12_8, result_12_16), ylim=c(330000, 336000))
dev.off ()
