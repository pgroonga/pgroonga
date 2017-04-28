ALTER OPERATOR FAMILY pgroonga.text_full_text_search_ops USING pgroonga
	ADD
		OPERATOR 12 &@ (text, text),
		OPERATOR 13 &? (text, text);
