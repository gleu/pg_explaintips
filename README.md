# README

## Requirements

The latest v18 pre-release of PostgreSQL (at the very least 4fd02bf7).

## Compilation

You only have to do:

```sh
make
make install
```

## Usage

The new option is called `TIPS`. It aims to provide tips on an explain plan.

Test case:

```sql
CREATE TABLE t1 (id integer);  
INSERT INTO t1 SELECT generate_series(1, 10_000);  
```

### Filtered rows

With a low ratio of filtered rows, no tip is offered.

```sql
EXPLAIN (ANALYZE,COSTS OFF,TIPS) SELECT * FROM t1 WHERE id>2;
```

```
Seq Scan on t1 (actual time=0.042..0.337 rows=998.00 loops=1)
  Filter: (id > 2)
  Rows Removed by Filter: 2
  Buffers: shared hit=5
Planning:
  Buffers: shared hit=4
Planning Time: 0.079 ms
Execution Time: 0.479 ms
```

With a high ratio of filtered rows, there's one tip:

```sql
EXPLAIN (ANALYZE,COSTS OFF,TIPS) SELECT * FROM t1 WHERE id<2;
```

```
Seq Scan on t1 (actual time=0.014..0.113 rows=1.00 loops=1)
  Filter: (id < 2)
  Rows Removed by Filter: 9999
  Buffers: shared hit=5
  Tips: You should probably add an index!
Planning Time: 0.035 ms
Execution Time: 0.127 ms
```

### VACUUM needed because of big heap fetches

With an index only scan, a big number of heap fetches means a VACUUM is needed.

Here is an example:

```sql
EXPLAIN (ANALYZE,COSTS OFF,TIPS) SELECT * FROM t1 WHERE id<1000;
```

```
Index Only Scan using t1_id_idx on t1 (actual time=0.021..0.077 rows=500.00 loops=1)
  Index Cond: (id < 1000)
  Heap Fetches: 179
  Index Searches: 1
  Buffers: shared hit=6
  Tips: You should probably VACUUM this relation!
Planning Time: 0.067 ms
Execution Time: 0.102 ms
```

### Not enough work_mem for sort

Not tip for a sort in memory:

```sql
EXPLAIN (ANALYZE,COSTS OFF,TIPS) SELECT * FROM t1 ORDER BY id;
```

```
Sort (actual time=1.216..1.559 rows=10000.00 loops=1)
  Sort Key: id
  Sort Method: quicksort  Memory: 385kB
  Buffers: shared hit=45
  ->  Seq Scan on t1 (actual time=0.009..0.477 rows=10000.00 loops=1)
        Buffers: shared hit=45
Planning Time: 0.034 ms
Execution Time: 1.911 ms
```

Tip for a sort on disk:

```sql
SET work_mem TO '64kB';
EXPLAIN (ANALYZE,COSTS OFF,TIPS) SELECT * FROM t1 ORDER BY id;
```

```
Sort (actual time=1.555..2.282 rows=10000.00 loops=1)
  Sort Key: id
  Sort Method: external merge  Disk: 144kB
  Buffers: shared hit=48, temp read=18 written=21
  Tips: You should probably increase work_mem!
  ->  Seq Scan on t1 (actual time=0.003..0.481 rows=10000.00 loops=1)
        Buffers: shared hit=45
Planning:
  Buffers: shared hit=30
Planning Time: 0.209 ms
Execution Time: 2.689 ms
```

