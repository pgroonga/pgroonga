SELECT pgroonga_highlight_html(
  'Groonga is a fast and accurate full text search engine based on ' ||
  'inverted index. One of the characteristics of Groonga is that a ' ||
  'newly registered document instantly appears in search results. ' ||
  'Also, Groonga allows updates without read locks. These characteristics ' ||
  'result in superior performance on real-time applications.' ||
  E'\n' ||
  E'\n' ||
  'Groonga is also a column-oriented database management system (DBMS). ' ||
  'Compared with well-known row-oriented systems, such as MySQL and ' ||
  'PostgreSQL, column-oriented systems are more suited for aggregate ' ||
  'queries. Due to this advantage, Groonga can cover weakness of ' ||
  'row-oriented systems.',
  ARRAY['fast', 'PostgreSQL']);
