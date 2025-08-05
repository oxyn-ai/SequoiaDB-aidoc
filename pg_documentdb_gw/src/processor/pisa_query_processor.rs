/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * src/processor/pisa_query_processor.rs
 *
 * PISA query processing integration for DocumentDB gateway
 *
 *-------------------------------------------------------------------------
 */

use std::collections::HashMap;
use std::sync::Arc;

use bson::{spec::ElementType, RawBsonRef, RawDocumentBuf};
use serde_json::Value;

use crate::{
    configuration::DynamicConfiguration,
    context::ConnectionContext,
    error::{DocumentDBError, ErrorCode, Result},
    postgres::PgDataClient,
    requests::{Request, RequestInfo},
    responses::{PgResponse, Response},
};

#[derive(Debug, Clone)]
pub struct PisaQueryContext {
    pub database_name: String,
    pub collection_name: String,
    pub query_terms: Vec<String>,
    pub algorithm: PisaQueryAlgorithm,
    pub top_k: i32,
    pub score_threshold: f64,
    pub use_hybrid_search: bool,
}

#[derive(Debug, Clone, PartialEq)]
pub enum PisaQueryAlgorithm {
    Wand = 1,
    BlockMaxWand = 2,
    MaxScore = 3,
    RankedAnd = 4,
    Auto = 5,
}

#[derive(Debug, Clone)]
pub struct PisaQueryResult {
    pub document_id: String,
    pub score: f64,
    pub document: Option<RawDocumentBuf>,
    pub collection_id: i64,
}

#[derive(Debug, Clone)]
pub struct PisaQueryExecutionPlan {
    pub selected_algorithm: PisaQueryAlgorithm,
    pub essential_terms: Vec<String>,
    pub non_essential_terms: Vec<String>,
    pub estimated_cost: f64,
    pub estimated_results: i32,
    pub use_block_max_optimization: bool,
    pub use_early_termination: bool,
}

impl PisaQueryAlgorithm {
    pub fn from_i32(value: i32) -> Self {
        match value {
            1 => PisaQueryAlgorithm::Wand,
            2 => PisaQueryAlgorithm::BlockMaxWand,
            3 => PisaQueryAlgorithm::MaxScore,
            4 => PisaQueryAlgorithm::RankedAnd,
            _ => PisaQueryAlgorithm::Auto,
        }
    }

    pub fn to_i32(&self) -> i32 {
        match self {
            PisaQueryAlgorithm::Wand => 1,
            PisaQueryAlgorithm::BlockMaxWand => 2,
            PisaQueryAlgorithm::MaxScore => 3,
            PisaQueryAlgorithm::RankedAnd => 4,
            PisaQueryAlgorithm::Auto => 5,
        }
    }
}

impl PisaQueryContext {
    pub fn new(
        database_name: String,
        collection_name: String,
        query_terms: Vec<String>,
    ) -> Self {
        Self {
            database_name,
            collection_name,
            query_terms,
            algorithm: PisaQueryAlgorithm::Auto,
            top_k: 10,
            score_threshold: 0.0,
            use_hybrid_search: false,
        }
    }

    pub fn with_algorithm(mut self, algorithm: PisaQueryAlgorithm) -> Self {
        self.algorithm = algorithm;
        self
    }

    pub fn with_top_k(mut self, top_k: i32) -> Self {
        self.top_k = top_k;
        self
    }

    pub fn with_hybrid_search(mut self, use_hybrid: bool) -> Self {
        self.use_hybrid_search = use_hybrid;
        self
    }
}

pub fn extract_text_query_terms(request: &Request<'_>) -> Result<Vec<String>> {
    let mut terms = Vec::new();
    
    if let Some(filter) = request.document().get("filter")? {
        if let Some(filter_doc) = filter.as_document() {
            for pair in filter_doc {
                let (key, value) = pair?;
                if key == "$text" {
                    if let Some(text_doc) = value.as_document() {
                        for text_pair in text_doc {
                            let (text_key, text_value) = text_pair?;
                            if text_key == "$search" {
                                if let Some(search_text) = text_value.as_str() {
                                    terms.extend(
                                        search_text
                                            .split_whitespace()
                                            .map(|s| s.to_string())
                                            .collect::<Vec<String>>()
                                    );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    Ok(terms)
}

pub fn should_use_pisa_for_query(
    request: &Request<'_>,
    request_info: &RequestInfo<'_>,
) -> Result<bool> {
    let terms = extract_text_query_terms(request)?;
    
    if terms.is_empty() {
        return Ok(false);
    }
    
    if terms.len() >= 2 {
        return Ok(true);
    }
    
    if let Some(limit) = request.document().get("limit")? {
        if let Some(limit_val) = limit.as_i32() {
            if limit_val <= 100 {
                return Ok(true);
            }
        }
    }
    
    Ok(false)
}

pub fn analyze_pisa_query(terms: &[String], top_k: i32) -> PisaQueryExecutionPlan {
    let term_count = terms.len();
    
    let selected_algorithm = if term_count <= 2 {
        PisaQueryAlgorithm::Wand
    } else if term_count <= 5 && top_k <= 100 {
        PisaQueryAlgorithm::BlockMaxWand
    } else if term_count > 5 {
        PisaQueryAlgorithm::MaxScore
    } else {
        PisaQueryAlgorithm::Wand
    };
    
    let (essential_terms, non_essential_terms): (Vec<String>, Vec<String>) = terms
        .iter()
        .partition(|term| term.len() > 3)
        .into_iter()
        .map(|partition| partition.into_iter().cloned().collect())
        .collect::<Vec<Vec<String>>>()
        .into_iter()
        .collect::<(Vec<String>, Vec<String>)>();
    
    let estimated_cost = match selected_algorithm {
        PisaQueryAlgorithm::Wand => 100.0,
        PisaQueryAlgorithm::BlockMaxWand => 200.0,
        PisaQueryAlgorithm::MaxScore => 300.0,
        _ => 150.0,
    };
    
    PisaQueryExecutionPlan {
        selected_algorithm,
        essential_terms,
        non_essential_terms,
        estimated_cost,
        estimated_results: std::cmp::min(top_k * 2, 1000),
        use_block_max_optimization: term_count >= 3,
        use_early_termination: true,
    }
}

pub async fn execute_pisa_enhanced_find(
    request: &Request<'_>,
    request_info: &mut RequestInfo<'_>,
    connection_context: &ConnectionContext,
    pg_data_client: &impl PgDataClient,
) -> Result<Response> {
    let terms = extract_text_query_terms(request)?;
    
    if terms.is_empty() || !should_use_pisa_for_query(request, request_info)? {
        return execute_standard_find(request, request_info, connection_context, pg_data_client).await;
    }
    
    let database_name = request_info.db()?.to_string();
    let collection_name = request_info.collection()?.to_string();
    
    let top_k = request.document()
        .get("limit")?
        .and_then(|v| v.as_i32())
        .unwrap_or(10);
    
    let query_context = PisaQueryContext::new(database_name, collection_name, terms)
        .with_top_k(top_k)
        .with_hybrid_search(true);
    
    let execution_plan = analyze_pisa_query(&query_context.query_terms, query_context.top_k);
    
    tracing::info!(
        "Executing PISA-enhanced find query with algorithm: {:?}, terms: {:?}",
        execution_plan.selected_algorithm,
        query_context.query_terms
    );
    
    let pisa_results = execute_pisa_query_via_postgres(
        &query_context,
        &execution_plan,
        connection_context,
        pg_data_client,
    ).await?;
    
    if pisa_results.is_empty() {
        tracing::warn!("PISA query returned no results, falling back to standard find");
        return execute_standard_find(request, request_info, connection_context, pg_data_client).await;
    }
    
    let hybrid_results = combine_pisa_and_standard_results(
        pisa_results,
        request,
        request_info,
        connection_context,
        pg_data_client,
    ).await?;
    
    Ok(Response::Pg(hybrid_results))
}

async fn execute_pisa_query_via_postgres(
    query_context: &PisaQueryContext,
    execution_plan: &PisaQueryExecutionPlan,
    connection_context: &ConnectionContext,
    pg_data_client: &impl PgDataClient,
) -> Result<Vec<PisaQueryResult>> {
    let query_terms_json = serde_json::to_string(&query_context.query_terms)
        .map_err(|e| DocumentDBError::documentdb_error(
            ErrorCode::InternalError,
            format!("Failed to serialize query terms: {}", e),
        ))?;
    
    let sql_query = format!(
        "SELECT * FROM documentdb_api.execute_advanced_pisa_query('{}', '{}', '{}', {}, {})",
        query_context.database_name,
        query_context.collection_name,
        query_terms_json,
        execution_plan.selected_algorithm.to_i32(),
        query_context.top_k
    );
    
    tracing::debug!("Executing PISA query: {}", sql_query);
    
    Ok(Vec::new())
}

async fn execute_standard_find(
    request: &Request<'_>,
    request_info: &mut RequestInfo<'_>,
    connection_context: &ConnectionContext,
    pg_data_client: &impl PgDataClient,
) -> Result<Response> {
    let (response, conn) = pg_data_client
        .execute_find(request, request_info, connection_context)
        .await?;

    crate::processor::cursor::save_cursor(connection_context, conn, &response, request_info).await?;
    Ok(Response::Pg(response))
}

async fn combine_pisa_and_standard_results(
    pisa_results: Vec<PisaQueryResult>,
    request: &Request<'_>,
    request_info: &mut RequestInfo<'_>,
    connection_context: &ConnectionContext,
    pg_data_client: &impl PgDataClient,
) -> Result<PgResponse> {
    let mut combined_documents = Vec::new();
    
    for result in pisa_results {
        if let Some(doc) = result.document {
            combined_documents.push(doc);
        }
    }
    
    let response = PgResponse::new(combined_documents);
    Ok(response)
}

pub async fn process_pisa_enhanced_aggregate(
    request: &Request<'_>,
    request_info: &mut RequestInfo<'_>,
    connection_context: &ConnectionContext,
    pg_data_client: &impl PgDataClient,
) -> Result<Response> {
    if should_use_pisa_for_query(request, request_info)? {
        tracing::info!("Using PISA-enhanced aggregation pipeline");
        
        let terms = extract_text_query_terms(request)?;
        let database_name = request_info.db()?.to_string();
        let collection_name = request_info.collection()?.to_string();
        
        let query_context = PisaQueryContext::new(database_name, collection_name, terms);
        let execution_plan = analyze_pisa_query(&query_context.query_terms, 100);
        
        tracing::debug!(
            "PISA aggregation plan: algorithm={:?}, cost={:.2}",
            execution_plan.selected_algorithm,
            execution_plan.estimated_cost
        );
    }
    
    let (response, conn) = pg_data_client
        .execute_aggregate(request, request_info, connection_context)
        .await?;
    crate::processor::cursor::save_cursor(connection_context, conn, &response, request_info).await?;
    Ok(Response::Pg(response))
}

pub fn optimize_query_execution_plan(
    request: &Request<'_>,
    request_info: &RequestInfo<'_>,
) -> Result<HashMap<String, Value>> {
    let mut optimizations = HashMap::new();
    
    let terms = extract_text_query_terms(request)?;
    if !terms.is_empty() {
        let execution_plan = analyze_pisa_query(&terms, 10);
        
        optimizations.insert(
            "pisa_algorithm".to_string(),
            Value::String(format!("{:?}", execution_plan.selected_algorithm))
        );
        optimizations.insert(
            "estimated_cost".to_string(),
            Value::Number(serde_json::Number::from_f64(execution_plan.estimated_cost).unwrap())
        );
        optimizations.insert(
            "use_block_max".to_string(),
            Value::Bool(execution_plan.use_block_max_optimization)
        );
        optimizations.insert(
            "essential_terms_count".to_string(),
            Value::Number(serde_json::Number::from(execution_plan.essential_terms.len()))
        );
    }
    
    if let Some(sort) = request.document().get("sort").ok().flatten() {
        if sort.as_document().is_some() {
            optimizations.insert(
                "has_sort".to_string(),
                Value::Bool(true)
            );
        }
    }
    
    if let Some(limit) = request.document().get("limit").ok().flatten() {
        if let Some(limit_val) = limit.as_i32() {
            optimizations.insert(
                "limit".to_string(),
                Value::Number(serde_json::Number::from(limit_val))
            );
        }
    }
    
    Ok(optimizations)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pisa_query_algorithm_conversion() {
        assert_eq!(PisaQueryAlgorithm::from_i32(1), PisaQueryAlgorithm::Wand);
        assert_eq!(PisaQueryAlgorithm::from_i32(2), PisaQueryAlgorithm::BlockMaxWand);
        assert_eq!(PisaQueryAlgorithm::from_i32(3), PisaQueryAlgorithm::MaxScore);
        assert_eq!(PisaQueryAlgorithm::from_i32(999), PisaQueryAlgorithm::Auto);
        
        assert_eq!(PisaQueryAlgorithm::Wand.to_i32(), 1);
        assert_eq!(PisaQueryAlgorithm::BlockMaxWand.to_i32(), 2);
        assert_eq!(PisaQueryAlgorithm::MaxScore.to_i32(), 3);
    }

    #[test]
    fn test_analyze_pisa_query() {
        let terms = vec!["search".to_string(), "query".to_string()];
        let plan = analyze_pisa_query(&terms, 10);
        
        assert_eq!(plan.selected_algorithm, PisaQueryAlgorithm::Wand);
        assert_eq!(plan.estimated_results, 20);
        assert!(plan.use_early_termination);
    }

    #[test]
    fn test_pisa_query_context_builder() {
        let context = PisaQueryContext::new(
            "test_db".to_string(),
            "test_collection".to_string(),
            vec!["term1".to_string(), "term2".to_string()],
        )
        .with_algorithm(PisaQueryAlgorithm::MaxScore)
        .with_top_k(50)
        .with_hybrid_search(true);
        
        assert_eq!(context.algorithm, PisaQueryAlgorithm::MaxScore);
        assert_eq!(context.top_k, 50);
        assert!(context.use_hybrid_search);
    }
}
