ALIGN_BITS <- c (0, 1, 2, 3, 4)
RANDOM_BITS <- c (6, 12)

# Read input files

read_results <- function (name)
{
  results <- c ()
  files <- list.files (name, full.names=TRUE)
  for (file in files)
  {
    lines <- readLines (file)
    line <- grep ("Operations performed: [0-9]+ \\([0-9.]+ ops/sec\\)", lines, value=TRUE)
    result <- as.numeric (sub (".*\\(([0-9.]+) ops/sec\\).*", "\\1", line))
    if ((length (result) == 1) && (result > 0)) results <- c (results, result)
  }
  
  return (results)
}

results_vanilla <- read_results ("sysbench-vanilla")

for (align in ALIGN_BITS)
{
  for (random in RANDOM_BITS)
  {
    name_directory = paste ("sysbench", "randomized", align, random, sep="-")
    name_variable = paste ("results", align, random, sep="_")
    assign (name_variable, read_results (name_directory))
  }
}

# Plot observations

plot_graph <- function (name, align_list, random)
{
  variable_list <- c ("results_vanilla", paste ("results", align_list, random, sep="_"))
  length_list <- unlist (lapply (variable_list, function (x) length(get (x))))
  horizontal_limits <- c (1, max (length_list)) 
  value_list <- unlist (lapply (variable_list, get))
  minimum <- min (value_list)
  maximum <- max (value_list)
  vertical_limits <- c (minimum, maximum)

  postscript (paste (name, "ps", sep="."))

  plot (c (), xlim = horizontal_limits, ylim = vertical_limits, ylab="Performance")
  for (variable_index in 1:length (variable_list))
  {
    variable = variable_list [variable_index]
    points (get (variable), pch = variable_index, col = variable_index)
  }

  legend (
    "bottomright",
    variable_list,
    pch = 1:length (variable_list),
    col = 1:length (variable_list))

  dev.off ()
}

plot_graph ("random-cache-line", ALIGN_BITS, 6)
plot_graph ("random-page-frame", ALIGN_BITS, 12)
