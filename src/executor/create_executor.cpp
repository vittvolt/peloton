//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// create_executor.cpp
//
// Identification: src/executor/create_executor.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "executor/create_executor.h"
#include "executor/executor_context.h"
#include "common/logger.h"
#include "catalog/catalog.h"
#include "catalog/foreign_key.h"
#include "type/types.h"

#include <vector>

namespace peloton {
namespace executor {

// Constructor for drop executor
CreateExecutor::CreateExecutor(const planner::AbstractPlan *node,
                               ExecutorContext *executor_context)
    : AbstractExecutor(node, executor_context) {
  context = executor_context;
}

// Initialize executer
// Nothing to initialize now
bool CreateExecutor::DInit() {
  LOG_TRACE("Initializing Create Executer...");
  LOG_TRACE("Create Executer initialized!");
  return true;
}

bool CreateExecutor::DExecute() {
  LOG_TRACE("Executing Create...");
  const planner::CreatePlan &node = GetPlanNode<planner::CreatePlan>();
  auto current_txn = context->GetTransaction();

  // Check if query was for creating table
  if (node.GetCreateType() == CreateType::TABLE) {
    std::string table_name = node.GetTableName();
    auto database_name = node.GetDatabaseName();
    std::unique_ptr<catalog::Schema> schema(node.GetSchema());

    ResultType result = catalog::Catalog::GetInstance()->CreateTable(
        database_name, table_name, std::move(schema), current_txn);
    current_txn->SetResult(result);

    if (current_txn->GetResult() == ResultType::SUCCESS) {
      LOG_TRACE("Creating table succeeded!");

      // Add the foreign key constraint (or other multi-column constriants)
      if (node.GetForeignKeys() != nullptr) {
        auto catalog = catalog::Catalog::GetInstance();
        auto source_table = catalog->GetDatabaseWithName(database_name)
                    ->GetTableWithName(table_name);
        int count = 1;
        for (auto &fk : *(node.GetForeignKeys())) {
          source_table->AddForeignKey(new catalog::ForeignKey(fk));

          // Register FK with the sink table for delete/update actions
          std::string sink_table_name = fk.GetSinkTableName();
          auto sink_table = catalog->GetDatabaseWithName(database_name)
                    ->GetTableWithName(sink_table_name);
          sink_table->RegisterForeignKeySource(table_name);

          // Add a non-unique index on the source table if needed
          if (fk.GetUpdateAction() != FKConstrActionType::NOACTION ||
              fk.GetDeleteAction() != FKConstrActionType::NOACTION) {
            std::vector<std::string> source_col_names = fk.GetFKColumnNames();
            std::string index_name =
                source_table->GetName() + "_FK_" + std::to_string(count);
            catalog->CreateIndex(database_name, source_table->GetName(), source_col_names,
                index_name, false, IndexType::BWTREE, current_txn);
            LOG_DEBUG("Added a FOREIGN index on in %s.", table_name.c_str());
            LOG_DEBUG("Foreign key column names: ");
            for (auto c : source_col_names) {
              LOG_DEBUG("FK col name: %s", c.c_str());
            }
            count++;
          }
        }
      }
    } else if (current_txn->GetResult() == ResultType::FAILURE) {
      LOG_TRACE("Creating table failed!");
    } else {
      LOG_TRACE("Result is: %s",
                ResultTypeToString(current_txn->GetResult()).c_str());
    }
  }

  // Check if query was for creating index
  if (node.GetCreateType() == CreateType::INDEX) {
    std::string table_name = node.GetTableName();
    std::string index_name = node.GetIndexName();
    bool unique_flag = node.IsUnique();
    IndexType index_type = node.GetIndexType();

    auto index_attrs = node.GetIndexAttributes();

    ResultType result = catalog::Catalog::GetInstance()->CreateIndex(
        DEFAULT_DB_NAME, table_name, index_attrs, index_name, unique_flag,
        index_type, current_txn);
    current_txn->SetResult(result);

    if (current_txn->GetResult() == ResultType::SUCCESS) {
      LOG_TRACE("Creating table succeeded!");
    } else if (current_txn->GetResult() == ResultType::FAILURE) {
      LOG_TRACE("Creating table failed!");
    } else {
      LOG_TRACE("Result is: %s",
                ResultTypeToString(current_txn->GetResult()).c_str());
    }
  }
  return false;
}
}
}
