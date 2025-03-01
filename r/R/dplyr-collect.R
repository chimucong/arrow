# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.


# The following S3 methods are registered on load if dplyr is present

collect.arrow_dplyr_query <- function(x, as_data_frame = TRUE, ...) {
  # See query-engine.R for ExecPlan/Nodes
  tab <- do_exec_plan(x)
  if (as_data_frame) {
    df <- as.data.frame(tab)
    tab$invalidate()
    restore_dplyr_features(df, x)
  } else {
    restore_dplyr_features(tab, x)
  }
}
collect.ArrowTabular <- function(x, as_data_frame = TRUE, ...) {
  if (as_data_frame) {
    as.data.frame(x, ...)
  } else {
    x
  }
}
collect.Dataset <- function(x, ...) dplyr::collect(as_adq(x), ...)

compute.arrow_dplyr_query <- function(x, ...) dplyr::collect(x, as_data_frame = FALSE)
compute.ArrowTabular <- function(x, ...) x
compute.Dataset <- compute.arrow_dplyr_query

pull.arrow_dplyr_query <- function(.data, var = -1) {
  .data <- as_adq(.data)
  var <- vars_pull(names(.data), !!enquo(var))
  .data$selected_columns <- set_names(.data$selected_columns[var], var)
  dplyr::collect(.data)[[1]]
}
pull.Dataset <- pull.ArrowTabular <- pull.arrow_dplyr_query

# TODO: Correctly handle group_vars after summarize; also in collapse()
restore_dplyr_features <- function(df, query) {
  # An arrow_dplyr_query holds some attributes that Arrow doesn't know about
  # After calling collect(), make sure these features are carried over

  if (length(query$group_by_vars) > 0) {
    # Preserve groupings, if present
    if (is.data.frame(df)) {
      df <- dplyr::grouped_df(
        df,
        dplyr::group_vars(query),
        drop = dplyr::group_by_drop_default(query)
      )
    } else {
      # This is a Table, via compute() or collect(as_data_frame = FALSE)
      df <- as_adq(df)
      df$group_by_vars <- query$group_by_vars
      df$drop_empty_groups <- query$drop_empty_groups
    }
  }
  df
}

collapse.arrow_dplyr_query <- function(x, ...) {
  # Figure out what schema will result from the query
  x$schema <- implicit_schema(x)
  # Nest inside a new arrow_dplyr_query
  arrow_dplyr_query(x)
}
collapse.Dataset <- collapse.ArrowTabular <- function(x, ...) {
  arrow_dplyr_query(x)
}

implicit_schema <- function(.data) {
  .data <- ensure_group_vars(.data)
  old_schm <- .data$.data$schema

  if (is.null(.data$aggregations)) {
    new_fields <- map(.data$selected_columns, ~ .$type(old_schm))
  } else {
    new_fields <- map(summarize_projection(.data), ~ .$type(old_schm))
    # * Put group_by_vars first (this can't be done by summarize,
    #   they have to be last per the aggregate node signature,
    #   and they get projected to this order after aggregation)
    # * Infer the output types from the aggregations
    group_fields <- new_fields[.data$group_by_vars]
    agg_fields <- imap(
      new_fields[setdiff(names(new_fields), .data$group_by_vars)],
      ~ output_type(.data$aggregations[[.y]][["fun"]], .x)
    )
    new_fields <- c(group_fields, agg_fields)
  }
  schema(!!!new_fields)
}
