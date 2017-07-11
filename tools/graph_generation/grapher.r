args = commandArgs(trailingOnly=TRUE)

if (length(args)!=1) {
	stop("this script requires only one mandatory argument: the working directory", call.=FALSE)
}

setwd(args[1])

list.of.packages <- c("ggplot2", "plyr")
new.packages <- list.of.packages[!(list.of.packages %in% installed.packages()[,"Package"])]

if(length(new.packages)) {
	install.packages(new.packages)
}

library("ggplot2")
library("plyr")

ue4_stats <- read.csv("ue4_stats.csv", row.names = NULL)
acl_stats <- read.csv("acl_stats.csv", row.names = NULL)

name <- factor(rep(c("ue4", "acl"), times = c(nrow(ue4_stats), nrow(acl_stats))))
compression_ratio <- c(ue4_stats$Compression.Ratio, acl_stats$Compression.Ratio)
max_error <- c(ue4_stats$Max.Error, acl_stats$Max.Error)

bundle <- data.frame(name, compression_ratio, max_error)
mu_ratio <- ddply(bundle, "name", summarise, grp.mean = mean(compression_ratio))
mu_error <- ddply(bundle, "name", summarise, grp.mean = mean(max_error))

p1 <- ggplot(ue4_stats, aes(x = Compression.Ratio)) +
  geom_histogram(color = "black", fill = "white") +
  geom_vline(aes(xintercept = mean(Compression.Ratio)), color = "blue", linetype = "dashed", size = 1)

png(filename = "ue4_compression_ratio.png")
plot(p1)
dev.off()

p2 <- ggplot(ue4_stats, aes(x = Max.Error)) +
  geom_histogram(color = "black", fill = "white") +
  geom_vline(aes(xintercept = mean(Max.Error)), color = "blue", linetype = "dashed", size = 1)

png(filename = "ue4_max_error.png")
plot(p2)
dev.off()

p3 <- ggplot(acl_stats, aes(x = Compression.Ratio)) +
  geom_histogram(color = "black", fill = "white") +
  geom_vline(aes(xintercept = mean(Compression.Ratio)), color = "blue", linetype = "dashed", size = 1)

png(filename = "acl_compression_ratio.png")
plot(p3)
dev.off()

p4 <- ggplot(acl_stats, aes(x = Max.Error)) +
  geom_histogram(color = "black", fill = "white") +
  geom_vline(aes(xintercept = mean(Max.Error)), color = "blue", linetype = "dashed", size = 1)

png(filename = "acl_max_error.png")
plot(p4)
dev.off()

p5 <- ggplot(bundle, aes(x = compression_ratio, color = name)) + geom_histogram(fill = "white", alpha = 0.5, position = "identity") +
geom_vline(data = mu_ratio, aes(xintercept = grp.mean, color = name), linetype = "dashed") +
theme(legend.position = "top")

png(filename = "ue4_acl_compression_ratio.png")
plot(p5)
dev.off()

p6 <- ggplot(bundle, aes(x = max_error, color = name)) + geom_histogram(fill = "white", alpha = 0.5, position = "identity") +
geom_vline(data = mu_error, aes(xintercept = grp.mean, color = name), linetype = "dashed") +
theme(legend.position = "top")

png(filename = "ue4_acl_max_error.png")
plot(p6)
dev.off()
