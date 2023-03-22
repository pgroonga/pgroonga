-- Upgrade SQL

CREATE OPERATOR CLASS pgroonga_uuid_ops DEFAULT FOR TYPE uuid
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;
