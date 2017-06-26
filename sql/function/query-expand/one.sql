CREATE TABLE full_text_sarch_engine (
  name text
);

INSERT INTO full_text_sarch_engine VALUES ('Groonga');
INSERT INTO full_text_sarch_engine VALUES ('Senna');
INSERT INTO full_text_sarch_engine VALUES ('Namazu');
INSERT INTO full_text_sarch_engine VALUES ('Hyper Estraier.');

SELECT pgroonga.query_expand('Groonga');
