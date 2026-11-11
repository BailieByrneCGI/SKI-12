#!/usr/bin/env Rscript
# Local, package-free analysis of the harness's text-free findings export.
args <- commandArgs(trailingOnly = TRUE)
option <- function(name, default = NULL) {
  pos <- match(name, args)
  if (is.na(pos)) return(default)
  if (pos == length(args) || startsWith(args[pos + 1L], "--"))
    stop(paste("Missing value for", name))
  args[pos + 1L]
}
input <- option("--findings")
output <- option("--output")
decisions_file <- option("--decisions")
fit_requested <- "--fit-model" %in% args
if (is.null(input) || is.null(output))
  stop("Usage: Rscript Tools/R/analyse.R --findings Output/Analytics/findings.csv --output Output/Analytics [--decisions Tools/R/reviewer_decisions.csv] [--fit-model]")
if (!file.exists(input)) stop("Findings export does not exist: ", input)
dir.create(output, showWarnings = FALSE, recursive = TRUE)

data <- read.csv(input, stringsAsFactors = FALSE, na.strings = c("", "NA"),
                 fileEncoding = "UTF-8", check.names = FALSE)
required <- c("case_id", "claim_id", "verdict", "severity", "assessment_method",
              "clause_id", "suggested_action", "flags", "retrieval_score",
              "model_confidence", "confidence_available", "review_required")
if (!all(required %in% names(data))) stop("Findings export has an incompatible schema")
if (nrow(data) == 0L || anyDuplicated(paste(data$case_id, data$claim_id, sep = "/")))
  stop("Findings export is empty or has duplicate case/claim keys")
if (any(is.na(data$retrieval_score)) || any(data$retrieval_score < 0))
  stop("Invalid retrieval score")
if (any(!is.na(data$model_confidence) &
        (data$model_confidence < 0 | data$model_confidence > 1)))
  stop("Invalid model confidence")
data$flags[is.na(data$flags)] <- ""
for (column in c("verdict", "severity", "assessment_method")) {
  data[[column]][is.na(data[[column]]) | data[[column]] == ""] <- "Unavailable"
}
data$clause_id[is.na(data$clause_id) | data$clause_id == ""] <- "No clause selected"
data$suggested_action[is.na(data$suggested_action) | data$suggested_action == ""] <- "No action"

count_table <- function(values, column) {
  if (length(values) == 0L) {
    result <- data.frame(label = character(), findings = integer())
    names(result)[1L] <- column
    return(result)
  }
  result <- as.data.frame(table(values), stringsAsFactors = FALSE)
  names(result) <- c(column, "findings")
  result[order(-result$findings, result[[column]]), , drop = FALSE]
}
write_count <- function(values, column, filename) {
  result <- count_table(values, column)
  write.csv(result, file.path(output, filename), row.names = FALSE, na = "")
  result
}
verdicts <- write_count(data$verdict, "verdict", "verdict_counts.csv")
actions <- write_count(data$suggested_action, "suggested_action", "action_counts.csv")
rules <- write_count(data$clause_id, "clause_id", "rule_counts.csv")
methods <- write_count(data$assessment_method, "assessment_method", "method_counts.csv")
all_flags <- unlist(strsplit(data$flags[nzchar(data$flags)], "|", fixed = TRUE),
                    use.names = FALSE)
flags <- write_count(all_flags, "flag", "flag_counts.csv")

plot_counts <- function(tab, file, title, colour = "#2E6171") {
  grDevices::png(file.path(output, file), width = 1400, height = 900, res = 150)
  on.exit(grDevices::dev.off())
  shown <- head(tab, 12L)
  if (nrow(shown) == 0L) {
    plot.new(); title(main = title); text(0.5, 0.5, "No observations")
  } else {
    par(mar = c(5, 11, 4, 2))
    labels <- substr(as.character(shown[[1L]]), 1L, 36L)
    barplot(rev(shown$findings), names.arg = rev(labels), horiz = TRUE,
            las = 1, col = colour, border = NA, main = title,
            xlab = "Findings", cex.names = 0.85)
  }
}
plot_counts(verdicts, "verdicts.png", "Finding assessments")
plot_counts(actions, "suggested_actions.png", "Suggested reviewer actions", "#B56B45")
plot_counts(rules, "cited_rules.png", "Selected clauses", "#527B49")
plot_counts(flags, "review_flags.png", "Review flags", "#765C89")

grDevices::png(file.path(output, "scores.png"), width = 1400, height = 900, res = 150)
tryCatch({
  par(mfrow = c(1, 2), mar = c(5, 5, 4, 2))
  hist(data$retrieval_score, col = "#2E6171", border = "white",
       main = "Rule retrieval score", xlab = "Lexical relevance score")
  confidence <- data$model_confidence[!is.na(data$model_confidence)]
  if (length(confidence)) {
    hist(confidence, breaks = seq(0, 1, 0.1), col = "#B56B45", border = "white",
         main = "Hermes reported confidence", xlab = "Uncalibrated self report", xlim = c(0, 1))
  } else {
    plot.new(); title(main = "Hermes reported confidence")
    text(0.5, 0.5, "No model confidence available")
  }
}, finally = grDevices::dev.off())

reviewed <- data.frame()
if (!is.null(decisions_file) && file.exists(decisions_file)) {
  decisions <- read.csv(decisions_file, stringsAsFactors = FALSE,
                        fileEncoding = "UTF-8", check.names = FALSE)
  expected <- c("case_id", "claim_id", "reviewer_decision", "reviewed_at")
  if (!all(expected %in% names(decisions))) stop("Reviewer decisions have an incompatible schema")
  keep <- !is.na(decisions$case_id) & !is.na(decisions$claim_id) &
          !is.na(decisions$reviewer_decision) & nzchar(decisions$case_id) &
          nzchar(decisions$claim_id) & nzchar(decisions$reviewer_decision)
  decisions <- decisions[keep, expected, drop = FALSE]
  if (anyDuplicated(paste(decisions$case_id, decisions$claim_id, sep = "/")))
    stop("Duplicate reviewer decision for one case/claim")
  if (!all(decisions$reviewer_decision %in% c("approve", "amend", "reject")))
    stop("Reviewer decision must be approve, amend, or reject")
  if (nrow(decisions)) {
    if (any(is.na(as.Date(decisions$reviewed_at, format = "%Y-%m-%d"))))
      stop("reviewed_at must use YYYY-MM-DD")
    key <- paste(data$case_id, data$claim_id, sep = "/")
    if (!all(paste(decisions$case_id, decisions$claim_id, sep = "/") %in% key))
      stop("Reviewer decision refers to a missing case/claim")
    reviewed <- merge(data, decisions, by = c("case_id", "claim_id"), sort = FALSE)
  }
}
decision_counts <- write_count(if (nrow(reviewed)) reviewed$reviewer_decision else character(),
                               "reviewer_decision", "reviewer_decision_counts.csv")
plot_counts(decision_counts, "reviewer_decisions.png", "Recorded human decisions", "#3F698C")

model_status <- "Not fitted: no supervised model was requested."
holdout_file <- file.path(output, "model_holdout.csv")
if (file.exists(holdout_file)) file.remove(holdout_file)
if (fit_requested) {
  complete <- reviewed[!is.na(reviewed$model_confidence), , drop = FALSE]
  complete$approved <- as.integer(complete$reviewer_decision == "approve")
  if (nrow(complete) < 100L || length(unique(complete$case_id)) < 100L ||
      sum(complete$approved == 1L) < 20L || sum(complete$approved == 0L) < 20L) {
    model_status <- paste("Not fitted: at least 100 distinct reviewed cases with model",
                          "confidence and at least 20 approvals and 20 amendments/rejections are required.")
  } else {
    case_dates <- aggregate(as.Date(complete$reviewed_at),
                            list(case_id = complete$case_id), min)
    names(case_dates)[2L] <- "reviewed_at"
    case_dates <- case_dates[order(case_dates$reviewed_at, case_dates$case_id), ]
    cut <- floor(nrow(case_dates) * 0.8)
    train <- complete[complete$case_id %in% case_dates$case_id[seq_len(cut)], ]
    test <- complete[!(complete$case_id %in% case_dates$case_id[seq_len(cut)]), ]
    if (length(unique(train$approved)) < 2L || nrow(test) < 20L) {
      model_status <- "Not fitted: the chronological train/test split has insufficient outcome variety."
    } else {
      model <- stats::glm(approved ~ retrieval_score + model_confidence,
                          family = stats::binomial(), data = train)
      predicted <- stats::predict(model, newdata = test, type = "response")
      if (any(!is.finite(predicted))) stop("Model produced non-finite predictions")
      brier <- mean((predicted - test$approved)^2)
      baseline <- mean((mean(train$approved) - test$approved)^2)
      model_status <- sprintf(paste("Exploratory reviewer-approval model: %d training findings,",
                                    "%d later test findings; Brier score %.3f versus %.3f baseline.",
                                    "These predictions are not case decisions."),
                              nrow(train), nrow(test), brier, baseline)
      write.csv(data.frame(case_id = test$case_id, claim_id = test$claim_id,
                           reviewed_at = test$reviewed_at, actual_approved = test$approved,
                           predicted_approval = predicted),
                holdout_file, row.names = FALSE)
    }
  }
}

lines <- c(
  "# Case analytics", "",
  sprintf("%d findings across %d cases.", nrow(data), length(unique(data$case_id))), "",
  sprintf("%d findings have a Hermes confidence value; %d have a recorded human review decision.",
          sum(!is.na(data$model_confidence)), nrow(reviewed)), "",
  "The charts count findings, not proven legal breaches. Suggested actions are Hermes proposals; only the separate reviewer decision file records human decisions. Model confidence is a self report, not a calibrated likelihood of a correct decision. Retrieval scores measure lexical relevance.", "",
  "## Charts", "",
  "- [Assessment counts](verdicts.png)",
  "- [Suggested actions](suggested_actions.png)",
  "- [Selected clauses](cited_rules.png)",
  "- [Review flags](review_flags.png)",
  "- [Score distributions](scores.png)",
  "- [Recorded human decisions](reviewer_decisions.png)", "",
  "## Exploratory model", "", model_status, "",
  "Source: findings.csv, exported from the latest per-case findings. Each row retains case_id and claim_id for audit back to Output/CASE-N. Complaint narrative, identities, prompts, and action detail are excluded from the export."
)
writeLines(lines, file.path(output, "report.md"), useBytes = TRUE)
cat(sprintf("R analysis complete: %d findings, %d cases, %d reviewed decisions\n",
            nrow(data), length(unique(data$case_id)), nrow(reviewed)))
