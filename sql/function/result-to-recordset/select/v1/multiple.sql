SELECT pgroonga_result_to_recordset(
  '[
     [0, 0.0, 0.0],
     [
       [
         [10],
         [
           ["_id", "UInt32"],
           ["bool", "Bool"],
           ["int8", "Int8"],
           ["uint8", "UInt8"],
           ["int16", "Int16"],
           ["uint16", "UInt16"],
           ["int32", "Int32"],
           ["uint32", "UInt32"],
           ["int64", "Int64"],
           ["uint64", "UInt64"],
           ["float32", "Float32"],
           ["float", "Float"],
           ["time", "Time"],
           ["short_text", "ShortText"],
           ["text", "Text"],
           ["long_text", "LongText"]
         ],
         [
           1,
           true,
           -128,
           255,
           -32768,
           65535,
           -2147483648,
           4294967295,
           -9223372036854775808,
           9223372036854775807,
           -1.0,
           1.0,
           1589102411.9,
           "short",
           "text",
           "long"
         ],
         [
           2,
           false,
           -10,
           10,
           -20,
           20,
           -30,
           30,
           -40,
           40,
           -5.5,
           5.5,
           1589102411,
           "short2",
           "text2",
           "long2"
         ]
       ]
     ]
   ]'::jsonb);
