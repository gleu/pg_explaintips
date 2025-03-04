/*-------------------------------------------------------------------------
 *
 * pg_explaintips.c
 *	  allow EXPLAIN to give some tips
 *
 * Copyright (c) 2025, Guillaume Lelarge, Dalibo
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_class.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "commands/explain_format.h"
#include "commands/explain_state.h"
#include "fmgr.h"
#include "parser/parsetree.h"
#include "storage/lock.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"

PG_MODULE_MAGIC;

typedef struct
{
	bool		tips;
} explaintips_options;

static explaintips_options *explaintips_ensure_options(ExplainState *es);
static void explaintips_handler(ExplainState *, DefElem *, ParseState *);
static void explaintips_per_node_hook(PlanState *planstate, List *ancestors,
									  const char *relationship,
									  const char *plan_name,
									  ExplainState *es);

static int	es_extension_id;
static explain_per_node_hook_type prev_explain_per_node_hook;
static int	filtered_rows_ratio = 300;

/*
 * Initialization we do when this module is loaded.
 */
void
_PG_init(void)
{
	/* Get an ID that we can use to cache data in an ExplainState. */
	es_extension_id = GetExplainExtensionId("pg_explaintips");

	/* Register the new EXPLAIN options implemented by this module. */
	RegisterExtensionExplainOption("tips", explaintips_handler);

	/* Use the per-node hook to make our options do something. */
	prev_explain_per_node_hook = explain_per_node_hook;
	explain_per_node_hook = explaintips_per_node_hook;

	DefineCustomIntVariable( "pg_explaintips.filtered_rows_ratio",
				"Ratio of filtered rows to add a tip for an index scan.",
				NULL,
				&filtered_rows_ratio,
				70, /* 70% by default */
				0,
				100,
				PGC_USERSET,
				0,
				NULL,
				NULL,
				NULL);
}

/*
 * Get the explaintips_options structure from an ExplainState; if there is
 * none, create one, attach it to the ExplainState, and return it.
 */
static explaintips_options *
explaintips_ensure_options(ExplainState *es)
{
	explaintips_options *options;

	options = GetExplainExtensionState(es, es_extension_id);

	if (options == NULL)
	{
		options = palloc0(sizeof(explaintips_options));
		SetExplainExtensionState(es, es_extension_id, options);
	}

	return options;
}

/*
 * Parse handler for EXPLAIN (TIPS).
 */
static void
explaintips_handler(ExplainState *es, DefElem *opt, ParseState *pstate)
{
	explaintips_options *options = explaintips_ensure_options(es);

	options->tips = defGetBoolean(opt);
}

/*
 * Print out additional per-node tips as appropriate. If the user didn't
 * specify any of the options we support, do nothing; else, print whatever
 * tips is relevant to the specific node.
 */
static void
explaintips_per_node_hook(PlanState *planstate, List *ancestors,
						  const char *relationship, const char *plan_name,
						  ExplainState *es)
{
	StringInfoData flags;
	explaintips_options *options;
	Plan	   *plan = planstate->plan;

	options = GetExplainExtensionState(es, es_extension_id);
	if (options == NULL)
		return;

	/*
	 * If the "tips" option was given, display tips.
	 */
	if (options->tips)
	{
		/*
		 * Tips for sequential scan with lots of filtered rows
		 */
		if (nodeTag(plan) == T_SeqScan)
		{
			double  rows = planstate->instrument->ntuples;
			double  nfiltered = planstate->instrument->nfiltered1;
			if (100*nfiltered/(nfiltered+rows) > filtered_rows_ratio)
			{
				initStringInfo(&flags);
				appendStringInfo(&flags, "You should probably add an index!");
				ExplainPropertyText("Tips", flags.data, es);
			}
		}

		/*
		 * Tips for disk sort
		 */
		if (nodeTag(plan) == T_Sort)
		{
			SortState *sortstate = castNode(SortState, planstate);
			Tuplesortstate *state = (Tuplesortstate *) sortstate->tuplesortstate;
			TuplesortInstrumentation stats;
			tuplesort_get_stats(state, &stats);
			if (stats.sortMethod == SORT_TYPE_EXTERNAL_SORT
				||
				stats.sortMethod == SORT_TYPE_EXTERNAL_MERGE)
			{
				initStringInfo(&flags);
				appendStringInfo(&flags, "You should probably increase work_mem!");
				ExplainPropertyText("Tips", flags.data, es);
			}
		}
	}
}

