ALTER OPERATOR FAMILY pgroonga.text_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 8 %% (text, text),
		OPERATOR 9 @@ (text, text);
