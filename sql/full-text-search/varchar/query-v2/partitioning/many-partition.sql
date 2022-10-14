CREATE TABLE infection_numbers_influenza (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
)
PARTITION BY LIST (city_code);

CREATE TABLE infection_numbers_influenza_20_01 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_20_01 FOR VALUES IN ('20-01');

CREATE TABLE infection_numbers_influenza_20_02 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_20_02 FOR VALUES IN ('20-02');

CREATE TABLE infection_numbers_influenza_20_03 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_20_03 FOR VALUES IN ('20-03');

CREATE TABLE infection_numbers_influenza_20_04 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_20_04 FOR VALUES IN ('20-04');

CREATE TABLE infection_numbers_influenza_21_01 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_21_01 FOR VALUES IN ('21-01');

CREATE TABLE infection_numbers_influenza_21_02 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_21_02 FOR VALUES IN ('21-02');

CREATE TABLE infection_numbers_influenza_21_03 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_21_03 FOR VALUES IN ('21-03');

CREATE TABLE infection_numbers_influenza_21_04 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_21_04 FOR VALUES IN ('21-04');

CREATE TABLE infection_numbers_influenza_22_01 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_22_01 FOR VALUES IN ('22-01');

CREATE TABLE infection_numbers_influenza_22_02 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_22_02 FOR VALUES IN ('22-02');

CREATE TABLE infection_numbers_influenza_22_03 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_22_03 FOR VALUES IN ('22-03');

CREATE TABLE infection_numbers_influenza_22_04 (
    country varchar(16) NOT NULL,
    prefecture varchar(4) NOT NULL,
    city varchar(3) NOT NULL,
    n_infection varchar(205),
    city_code varchar(5) NOT NULL,
    remarks varchar(200)
);
ALTER TABLE ONLY infection_numbers_influenza ATTACH PARTITION infection_numbers_influenza_22_04 FOR VALUES IN ('22-04');

INSERT INTO infection_numbers_influenza_20_04 VALUES ('Japan','Mie','Ise','134','20-04','age: 30, gender:male, overseas travel history: true');
INSERT INTO infection_numbers_influenza_21_04 VALUES ('Germany','BER','BER','221','21-04','age: 50, gender:male, overseas travel history: false');
INSERT INTO infection_numbers_influenza_22_04 VALUES ('U.S.A','NY','NY','10221','22-04','age: 10, gender:male, overseas travel history: false');

CREATE INDEX remarks_index ON infection_numbers_influenza USING pgroonga (remarks);

SELECT t1.n_infection, t1.remarks
  FROM infection_numbers_influenza t1
  INNER JOIN infection_numbers_influenza t2
    ON t1.city = t2.prefecture
 WHERE t1.remarks  &@~ 'age';

DROP TABLE infection_numbers_influenza;
