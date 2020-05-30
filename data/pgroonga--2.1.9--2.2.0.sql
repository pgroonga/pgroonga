-- Upgrade SQL

CREATE OPERATOR CLASS pgroonga_int4_array_ops DEFAULT FOR TYPE int4[]
	USING pgroonga AS
		OPERATOR 3 = (anyarray, anyarray);
