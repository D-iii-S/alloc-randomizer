ALIGN_BITS <- c (0, 1, 2, 3, 4)
RANDOM_BITS <- c (6, 12)

BENCHMARKS <- c ("400_perlbench", "401_bzip2", "403_gcc", "410_bwaves", "416_gamess", "429_mcf", "433_milc",
                 "434_zeusmp", "435_gromacs", "436_cactusADM", "437_leslie3d", "444_namd", "445_gobmk",
                 "447_dealII", "450_soplex", "453_povray", "454_calculix", "456_hmmer", "458_sjeng",
                 "459_GemsFDTD", "462_libquantum", "464_h264ref", "465_tonto", "470_lbm",
                 "471_omnetpp", "473_astar", "481_wrf", "482_sphinx3", "483_xalancbmk",
                 "998_specrand", "999_specrand")

read_results <- function (name, benchmark)
{
  results <- c ()
  files <- list.files (name, full.names=TRUE)
  for (file in files)
  {
    lines <- readLines (file)
    links <- grep ("format: raw -> [^ ]+\\.rsf", lines, value=TRUE)
    raws <- sub (" *format: raw -> ([^ ]+\\.rsf)", "\\1", links)

    for (raw in raws)
    {
      # Create local copies of the results for archival purposes
      local = paste (name, basename (raw), sep="/")
      if (!file.exists (local)) file.copy (raw, local)    
      
      # Always read the local copies of the results
      lines <- readLines (local)
      key <- paste ("spec", "cpu2006", "results", benchmark, "base", "000", "reported_time", sep=".")
      line <- grep (key, lines, value=TRUE)
      result <- as.numeric (sub (".*\\.reported_time: ([0-9.]+)", "\\1", line))
      if ((length (result) == 1) && (result > 0)) results <- c (results, result)
    }
  }
  
  return (results)
}

for (benchmark in BENCHMARKS)
{
  # Read input files

  results_vanilla <- read_results ("speccpu-vanilla", benchmark)

  for (align in ALIGN_BITS)
  {
	for (random in RANDOM_BITS)
	{
	  name_directory = paste ("speccpu", "randomized", align, random, sep="-")
	  name_variable = paste ("results", align, random, sep="_")
	  assign (name_variable, read_results (name_directory, benchmark))
	}
  }

  # Plot observations

  plot_graph <- function (name, benchmark, align_list, random)
  {
    variable_list <- c ("results_vanilla", paste ("results", align_list, random, sep="_"))
    length_list <- unlist (lapply (variable_list, function (x) length(get (x))))
    horizontal_limits <- c (1, max (length_list)) 
    value_list <- unlist (lapply (variable_list, get))
    minimum <- min (c (value_list, Inf))
    maximum <- max (c (value_list, -Inf))
    vertical_limits <- c (minimum, maximum)

    if (length (value_list) > 0)
    {
      postscript (paste (name, benchmark, "ps", sep="."))

      plot (c (), xlim = horizontal_limits, ylim = vertical_limits, ylab="Performance")
      for (variable_index in 1:length (variable_list))
      {
        variable = variable_list [variable_index]
        points (get (variable), pch = variable_index, col = variable_index)
      }

      legend (
        "topright",
        variable_list,
        pch = 1:length (variable_list),
        col = 1:length (variable_list))

      dev.off ()
    }
  }

  plot_graph ("random-cache-line", benchmark, ALIGN_BITS, 6)
  plot_graph ("random-page-frame", benchmark, ALIGN_BITS, 12)
}
