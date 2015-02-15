# PGroonga（ぴーじーるんが）

## リリース情報

[news.md](news.md)を参照してください。

## 概要

PGroongaはPostgreSQLからインデックスとして
[Groonga](http://groonga.org/ja/)を使うための拡張機能です。

PostgreSQLは標準では日本語で全文検索できませんが、PGroongaを使うと日本
語で高速に全文検索できるようになります。日本語で全文検索機能を実現する
ための類似の拡張機能は次のものがあります。

  * [pg_trgm](https://www.postgresql.jp/document/9.3/html/pgtrgm.html)
    * PostgreSQLに付属しているがデフォルトではインストールされない。
    * 日本語に対応させるにはソースコードを変更する必要がある。
  * [pg_bigm](http://pgbigm.sourceforge.jp/)
    * ソースコードを変更しなくても日本語に対応している。
    * 正確な全文検索機能を使うには
      [Recheck機能](http://pgbigm.sourceforge.jp/pg_bigm-1-1.html#enable_recheck)
      を有効にする必要がある。
    * Recheck機能を有効にするとインデックスを使った検索をしてから、イ
      ンデックスを使って見つかったレコードに対してシーケンシャルに検索
      をするのでインデックスを使った検索でのヒット件数が多くなると遅く
      なりやすい。
    * Recheck機能を無効にするとキーワードが含まれていないレコードもヒッ
      トする可能性がある。

PGroongaはpg\_trgmのようにソースコードを変更しなくても日本語に対応して
います。

PGroongaはpg\_bigmのようにRecheck機能を使わなくてもインデックスを使っ
た検索だけで正確な検索結果を返せます。そのため、インデックスを使った検
索でヒット件数が多くてもpg\_bigmほど遅くなりません。（仕組みの上は。要
ベンチマーク。協力者募集。）

ただし、PGroongaは現時点ではWALに対応していない(*)ためクラッシュリカバ
リー機能やレプリケーションに対応していません。（pg\_trgmとpg\_bigmは対
応しています。正確に言うとpg\_trgmとpg\_bigmが対応しているわけではなく、
pg\_trgmとpg\_bigmが使っているGINやGiSTが対応しています。）

(*) PostgreSQLは拡張機能として実装したインデックスがWALに対応するため
のAPIを提供していません。PostgreSQL本体がそんなAPIを提供したらWALに対
応する予定です。

## インストール

次の環境用のパッケージを用意しています。

  * Ubuntu 14.10
  * CentOS 7

その他の環境ではソースからインストールしてください。

それぞれの環境でのインストール方法の詳細は以降のセクションで説明します。

### Ubuntu 14.10にインストール

`postgresql-server-9.4-pgroonga`パッケージをインストールします。

    % sudo apt-get -y install software-properties-common
    % sudo add-apt-repository -y universe
    % sudo add-apt-repository -y ppa:groonga/ppa
    % sudo apt-get update
    % sudo apt-get -y install postgresql-server-9.4-pgroonga

データベースを作成します。

    % sudo -u postgres -H psql --command 'CREATE DATABASE pgroonga_test'

（ここで`pgroonga_test`用のユーザーを作成して、そのユーザーで接続する
べき。）

データベースに接続して`CREATE EXTENSION pgroonga`を実行します。

    % sudo -u postgres -H psql -d pgroonga_test --command 'CREATE EXTENSION pgroonga'

これでインストールは完了です。

### CentOS 7にインストール

`postgresql-pgroonga`パッケージをインストールします。

    % sudo rpm -ivh http://packages.groonga.org/centos/groonga-release-1.1.0-1.noarch.rpm
    % sudo yum makecache
    % sudo yum install -y postgresql-pgroonga

PostgreSQLを起動します。

    % sudo -H postgresql-setup initdb
    % sudo -H systemctl start postgresql

データベースを作成します。

    % sudo -u postgres -H psql --command 'CREATE DATABASE pgroonga_test'

（ここで`pgroonga_test`用のユーザーを作成して、そのユーザーで接続する
べき。）

データベースに接続して`CREATE EXTENSION pgroonga`を実行します。

    % sudo -u postgres -H psql -d pgroonga_test --command 'CREATE EXTENSION pgroonga'

これでインストールは完了です。

### ソースからインストール

PostgreSQLをインストールします。

[Groongaをインストール](http://groonga.org/ja/docs/install.html)します。
パッケージでのインストールがオススメです。

パッケージでインストールするときは次のパッケージをインストールしてください。

  * `groonga-devel`: CentOSの場合
  * `libgroonga-dev`: Debian GNU/Linux, Ubuntuの場合

PGroongaのソースを展開します。

リリース版の場合:

    % wget http://packages.groonga.org/source/pgroonga/pgroonga-0.3.0.tar.gz
    % tar xvf pgroonga-0.3.0.tar.gz
    % cd pgroonga-0.3.0

未リリースの最新版の場合:

    % git clone https://github.com/pgroonga/pgroonga.git
    % cd pgroonga

PGroongaをビルドしてインストールします。

    % make
    % sudo make install

データベースに接続して`CREATE EXTENSION pgroonga`を実行します。

    % psql -d db --command 'CREATE EXTENSION pgroonga;'

## 使い方

PGroongaは全文検索はもちろん、数値や文字列の等価条件（`=`）や比較条件
（`<`や`>=`など）にも使えます。

まずは全文検索の使い方について説明し、次に等価条件や比較条件で使う方法
を説明します。

### 全文検索

#### 基本的な使い方

`text`型のカラムを作って`pgroonga`インデックスを張ります。
（`varchar`型に対して全文検索をする場合は追加で
`pgroonga.varchar_fulltext_search_ops`演算子クラスを指定する必要があり
ます。）

```sql
CREATE TABLE memos (
  id integer,
  content text
);

CREATE INDEX pgroonga_content_index ON memos USING pgroonga (content);
```

データを投入します。

```sql
INSERT INTO memos VALUES (1, 'PostgreSQLはリレーショナル・データベース管理システムです。');
INSERT INTO memos VALUES (2, 'Groongaは日本語対応の高速な全文検索エンジンです。');
INSERT INTO memos VALUES (3, 'PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。');
INSERT INTO memos VALUES (4, 'groongaコマンドがあります。');
```

検索します。ここではシーケンシャルスキャンではなくインデックスを使った
全文検索を使いたいので、シーケンシャルスキャン機能を無効にします。（あ
るいはもっとたくさんのデータを投入します。）

```sql
SET enable_seqscan = off;
```

##### `%%`演算子

全文検索をするときは`%%`演算子を使います。

```sql
SELECT * FROM memos WHERE content %% '全文検索';
--  id |                      content
-- ----+---------------------------------------------------
--   2 | Groongaは日本語対応の高速な全文検索エンジンです。
-- (1 行)
```

##### `@@`演算子

`キーワード1 OR キーワード2`のようにクエリー構文を使って全文検索をする
ときは`@@`演算子を使います。

```sql
SELECT * FROM memos WHERE content @@ 'PGroonga OR PostgreSQL';
--  id |                                  content
-- ----+---------------------------------------------------------------------------
--   3 | PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。
--   1 | PostgreSQLはリレーショナル・データベース管理システムです。
-- (2 行)
```

クエリー構文の詳細は
[Groognaのドキュメント](http://groonga.org/ja/docs/reference/grn_expr/query_syntax.html)
を参照してください。

ただし、`カラム名:@キーワード`というように「`カラム名:`」から始まる構
文は無効にしてあります。そのため、前方一致検索をしたい場合は「`カラム
名:^値`」という構文は使えず、「`値*`」という構文だけが使えます。

注意してください。

##### `LIKE`演算子

既存のSQLを変更しなくても使えるように`column LIKE '%キーワード%'`と書
くと`column @@ 'キーワード'`と同等の処理になるようになっています。

なお、`'キーワード%'`や`'%キーワード'`のように最初と最後に`%`がついて
いない場合は必ず検索結果が空になります。このようなパターンはインデック
スを使って検索できないからです。気をつけてください。

本来の`LIKE`演算子は元の文字列そのものに対して検索しますが、`@@`演算子
は正規化後の文字列に対して全文検索検索を実行します。そのため、インデッ
クスを使わない場合の`LIKE`演算子の結果（本来の`LIKE`演算子の結果）とイ
ンデックスを使った場合の`LIKE`演算子の結果は異なります。

たとえば、本来の`LIKE`演算子ではアルファベットの大文字小文字を区別した
りいわゆる全角・半角を区別しますが、インデックスを使った場合は区別なく
検索します。

本来の`LIKE`演算子の結果:

```sql
SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;
SELECT * FROM memos WHERE content LIKE '%groonga%';
--  id |           content           
-- ----+-----------------------------
--   4 | groongaコマンドがあります。
-- (1 行)
```

インデックスを使った`LIKE`演算子の結果:

```sql
SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = on;
SELECT * FROM memos WHERE content LIKE '%groonga%';
--  id |                                  content                                  
-- ----+---------------------------------------------------------------------------
--   2 | Groongaは日本語対応の高速な全文検索エンジンです。
--   3 | PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。
--   4 | groongaコマンドがあります。
-- (3 行)
```

インデックスを使った場合でも本来の`LIKE`演算子と同様の結果にしたい場合
は次のようにトークナイザー（後述）とノーマライザー（後述）を設定してイ
ンデックスを作成してください。

  * トークナイザー: `TokenBigramSplitSymbolAlphaDigit`
  * ノーマライザー: なし

具体的には次のようにインデックスを作成します。

```sql
CREATE INDEX pgroonga_content_index
          ON memos
       USING pgroonga (content)
        WITH (tokenizer='TokenBigramSplitSymbolAlphaDigit',
              normalizer='');
```

このようなインデックスを作っているときはインデックスを使った`LIKE`演算
子でも本来の`LIKE`演算子と同様の挙動になります。

```sql
SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = on;
SELECT * FROM memos WHERE content LIKE '%groonga%';
--  id |           content           
-- ----+-----------------------------
--   4 | groongaコマンドがあります。
-- (1 行)
```

多くの場合、デフォルトの設定の全文検索結果の方が本来の`LIKE`演算子の方
の検索結果よりもユーザーが求めている結果に近くなります。本当に本来の
`LIKE`演算子の挙動の方が適切か検討した上で使ってください。

##### `pgroonga.score`関数

`pgroonga.score`関数を使うと該当レコードがどの程度クエリーに適合してい
るかを数値で取得することができます。

```sql
SELECT *, pgroonga.score(memos)
  FROM memos
 WHERE content %% 'PGroonga' OR content %% 'PostgreSQL';
--  id |                                  content                                  | score 
-- ----+---------------------------------------------------------------------------+-------
--   1 | PostgreSQLはリレーショナル・データベース管理システムです。                |     1
--   3 | PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。 |     2
-- (2 行)
```

`ORDER BY`句で`pgroonga.score`関数を使うことにより、適合度が高い順に並
び替えることができます。

```sql
SELECT *, pgroonga.score(memos)
  FROM memos
 WHERE content %% 'PGroonga' OR content %% 'PostgreSQL'
 ORDER BY pgroonga.score(memos) DESC;
--  id |                                  content                                  | score 
-- ----+---------------------------------------------------------------------------+-------
--   3 | PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。 |     2
--   1 | PostgreSQLはリレーショナル・データベース管理システムです。                |     1
-- (2 行)
```

現時点では適合度は「キーワードを含んでいる数」になります。Groongaには
キーワードや検索対象カラム毎に重みをつける機能や適合度の計算方法をカス
タマイズする機能があります。しかし、PostgreSQLらしく指定するAPIを思い
ついていないためPGroongaから使うことはできません。

#### カスタマイズ

`CREATE INDEX`の`WITH`でトークナイザーとノーマライザーをカスタマイズす
ることができます。デフォルトで適切なトークナイザーとノーマライザーが設
定されているので、通常はカスタマイズする必要はありません。上級者向けの
機能です。

なお、デフォルトのトークナイザーとノーマライザーは次の通りです。

  * トークナイザー: `TokenBigram`: Bigramベースのトークナイザーです。
  * ノーマライザー: [NormalizerAuto](http://groonga.org/ja/docs/reference/normalizers.html#normalizer-auto): エンコーディングに合わせて適切な正規化を行います。たとえば、UTF-8の場合はUnicodeのNFKCベースの正規化を行います。

トークナイザーをカスタマイズするときは`tokenizer='トークナイザー名'`を
指定します。例えば、
[MeCab](http://mecab.googlecode.com/svn/trunk/mecab/doc/index.html)ベー
スのトークナイザーを指定する場合は次のように`tokenizer='TokenMecab'`を
指定します。

```sql
CREATE TABLE memos (
  id integer,
  content text
);

CREATE INDEX pgroonga_content_index
          ON memos
       USING pgroonga (content)
        WITH (tokenizer='TokenMecab');
```

次のように`tokenizer=''`を指定することでトークナイザーを無効にできます。
トークナイザーを無効にするとカラムの値そのもの、あるいは値の前方一致検
索でのみヒットするようになります。これは、タグ検索や名前検索などに有用
です。（タグ検索には`tokenizer='TokenDelimit'`も有用です。）

```sql
CREATE TABLE memos (
  id integer,
  tag text
);

CREATE INDEX pgroonga_tag_index
          ON memos
       USING pgroonga (tag)
        WITH (tokenizer='');
```

ノーマライザーをカスタマイズするときは`normalizer='ノーマライザー名'`を
指定します。通常は指定する必要はありません。

次のように`normalizer=''`を指定することでノーマライザーを無効にできま
す。ノーマライザーを無効にするとカラムの値そのものでのみヒットするよう
になります。正規化によりノイズが増える場合は有用な機能です。

```sql
CREATE TABLE memos (
  id integer,
  tag text
);

CREATE INDEX pgroonga_tag_index
          ON memos
       USING pgroonga (tag)
        WITH (normalizer='');
```

### 等価・比較

等価・比較条件のためにPGroongaを使う方法は文字列型とそれ以外の型でイン
デックスの作成方法が少し違います。なお、検索条件（`WHERE`）の書き方は
B-treeを使ったインデックスなどのときと同じです。

文字列型以外での方の使い方が簡単なのでそちらから説明します。その後、文
字列型での使い方を説明します。

#### 数値など文字列型以外の型

文字列型以外の型を使うときは`USING`に`pgroonga`を指定してください。

```sql
CREATE TABLE ids (
  id integer
);

CREATE INDEX pgroonga_id_index ON ids USING pgroonga (id);
```

あとはB-treeを使ったインデックスなどのときと同じです。

データを投入します。

```sql
INSERT INTO ids VALUES (1);
INSERT INTO ids VALUES (2);
INSERT INTO ids VALUES (3);
```

シーケンシャルスキャンを無効にします。

```sql
SET enable_seqscan = off;
```

検索します。

```sql
SELECT * FROM ids WHERE id <= 2;
--  id
-- ----
--   1
--   2
-- (2 行)
```

#### 文字列型

文字列型に対して等価・比較条件を使う場合は`varchar`型に対してインデッ
クスを作成してください。最大バイト数が4096バイト以下になるように
`varchar`に最大文字数を指定してください。エンコーディングによって文字
数とバイト数の関係が変わることに注意してください。UTF-8を使っている場
合は最大文字数は1023になります。

```sql
CREATE TABLE tags (
  id integer,
  tag varchar(1023)
);

CREATE INDEX pgroonga_tag_index ON tags USING pgroonga (tag);
```

あとはB-treeを使ったインデックスなどのときと同じです。

データを投入します。

```sql
INSERT INTO tags VALUES (1, 'PostgreSQL');
INSERT INTO tags VALUES (2, 'Groonga');
INSERT INTO tags VALUES (3, 'Groonga');
```

シーケンシャルスキャンを無効にします。

```sql
SET enable_seqscan = off;
```

検索します。

```sql
SELECT * FROM tags WHERE tag = 'Groonga';
--  id |   tag
-- ----+---------
--   2 | Groonga
--   3 | Groonga
-- (2 行)
--
```

### Groongaの機能を使う

多くの場合、PostgreSQLよりGroongaの方が高速に処理できます。たとえば、
ドリルダウン機能を使うことにより検索結果の取得と複数の`GROUP BY`結果の
取得を1つのクエリーで実行することができるため、複数の`SELECT`文を実行
するよりも高速です。他にも、Groongaは列指向のデータストアを使っている
ため、一部のカラムだけを検索・取得する場合は行指向のデータストアの
PostgreSQLよりも高速です。

しかし、直接Groongaで検索するとSQLとは違うAPIになり、使い勝手がよくあ
りません。それでもGroongaを使いたい場合のためにSQL経由でGroongaを使う
機能を用意しています。

#### `pgroonga.command`関数

`pgroonga.command`関数を使うと
[Groongaのコマンド](http://groonga.org/ja/docs/reference/command.html)
を実行してその結果を文字列で取得できます。

次は
[statusコマンド](http://groonga.org/ja/docs/reference/commands/status.html)
を実行する例です。

```sql
SELECT pgroonga.command('status');
--                                   command                                                                                                                  
-- -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
--  [[0,1423911561.69344,6.15119934082031e-05],{"alloc_count":164,"starttime":1423911561,"uptime":0,"version":"5.0.0-6-g17847c9","n_queries":0,"cache_hit_rate":0.0,"command_version":1,"default_command_version":1,"max_command_version":2}]
-- (1 行)
```

Groongaのコマンドの実行結果はJSONなのでPostgreSQLのJSON関数を使って扱
いやすくすることができます。

たとえば、`status`コマンドの結果は次のようにするとそれぞれの値を1行と
して扱うことができます。

```sql
SELECT * FROM json_each(pgroonga.command('status')::json->1);
--            key           |       value        
-- -------------------------+--------------------
--  alloc_count             | 168
--  starttime               | 1423911561
--  uptime                  | 221
--  version                 | "5.0.0-6-g17847c9"
--  n_queries               | 0
--  cache_hit_rate          | 0.0
--  command_version         | 1
--  default_command_version | 1
--  max_command_version     | 2
-- (9 行)
```

#### `pgroonga.table_name`関数

PGroongaのインデックス対象のカラムの値はGroongaのデータベースにも保存
されています。そのため、Groongaの
[selectコマンド](http://groonga.org/ja/docs/reference/commands/select.html)
を使って検索し、値を出力することができます。

`select`コマンドを使うにはGroongaでのテーブル名が必要です。インデック
ス名をGroongaでのテーブル名に変換するには`pgroonga.table_name`関数を使
います。

`pgroonga.table_name`関数を使うと次のように`select`コマンドを使うこと
ができます。

```sql
SELECT *
  FROM json_array_elements(pgroonga.command('select ' || pgroonga.table_name('pgroonga_content_index'))::json->1->0);
--                                        value                                       
-- -----------------------------------------------------------------------------------
--  [4]
--  [["_id","UInt32"],["_key","UInt64"],["content","LongText"]]
--  [1,1,"PostgreSQLはリレーショナル・データベース管理システムです。"]
--  [2,2,"Groongaは日本語対応の高速な全文検索エンジンです。"]
--  [3,3,"PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。"]
--  [4,4,"groongaコマンドがあります。"]
-- (6 行)
```

`select`コマンドを使うとカラムに重みをつけることもできます。

例として次のようなスキーマとデータを使います。検索したいデータと出力し
たいデータを両方インデックス対象にしています。

```sql
CREATE TABLE terms (
  id integer,
  title text,
  content text,
  tag varchar(256)
);

CREATE INDEX pgroonga_terms_index
          ON terms
       USING pgroonga (title, content, tag);

INSERT INTO terms
     VALUES (1,
             'PostgreSQL',
             'PostgreSQLはリレーショナル・データベース管理システムです。',
             'PostgreSQL');
INSERT INTO terms
     VALUES (2,
             'Groonga',
             'Groongaは日本語対応の高速な全文検索エンジンです。',
             'Groonga');
INSERT INTO terms
     VALUES (3,
             'PGroonga',
             'PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。',
             'PostgreSQL');
```

[match_columnsオプション](http://groonga.org/ja/docs/reference/commands/select.html#select-match-columns)で重みを指定できます。

```sql
SELECT *
  FROM json_array_elements(
         pgroonga.command('select ' ||
                          pgroonga.table_name('pgroonga_terms_index') || ' ' ||
                          '--match_columns "title * 10 || content" ' ||
                          '--query "Groonga OR PostgreSQL OR 全文検索" ' ||
                          '--output_columns "_score, title, content" ' ||
                          '--sortby "-_score"'
                         )::json->1->0);
--                                            value                                            
-- --------------------------------------------------------------------------------------------
--  [3]
--  [["_score","Int32"],["title","LongText"],["content","LongText"]]
--  [12,"Groonga","Groongaは日本語対応の高速な全文検索エンジンです。"]
--  [11,"PostgreSQL","PostgreSQLはリレーショナル・データベース管理システムです。"]
--  [2,"PGroonga","PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。"]
-- (5 行)
```

[drilldownオプション](http://groonga.org/ja/docs/reference/commands/select.html#select-drilldown)
を加えるとドリルダウン結果も取得できます。

```sql
SELECT *
  FROM json_array_elements(
         pgroonga.command('select ' ||
                          pgroonga.table_name('pgroonga_terms_index') || ' ' ||
                          '--match_columns "title * 10 || content" ' ||
                          '--query "Groonga OR PostgreSQL OR 全文検索" ' ||
                          '--output_columns "_score, title" ' ||
                          '--sortby "-_score" ' ||
                          '--drilldown "tag"'
                         )::json->1);
--                                               value                                              
-- -------------------------------------------------------------------------------------------------
--  [[3],[["_score","Int32"],["title","LongText"]],[12,"Groonga"],[11,"PostgreSQL"],[2,"PGroonga"]]
--  [[2],[["_key","ShortText"],["_nsubrecs","Int32"]],["Groonga",1],["PostgreSQL",2]]
-- (2 行)
```

SQLの`SELECT`文でどうにもならなくなったときに、もしかしたらGroongaの
`select`コマンドを使えるかもしれません。

## アンインストール

次のSQLでアンインストールできます。

```sql
DROP EXTENSION pgroonga CASCADE;
DELETE FROM pg_catalog.pg_am WHERE amname = 'pgroonga';
```

`pg_catalog.pg_am`から手動でレコードを消さないといけないのはおかしい気
がするので、何がおかしいか知っている人は教えてくれるとうれしいです。

## Travis CIで使う

Travis CIでのPGroongaのセットアップをするシェルスクリプトを提供してい
ます。次のように`.travis.yml`の`install`で使うようにしてください。

```yaml
install:
  - curl --silent --location https://github.com/pgroonga/pgroonga/raw/master/data/travis/setup.sh | sh
```

このシェルスクリプトの中では`template1`データベースに対して`CREATE
EXTENSION pgroonga`をしているので、新しくデータベースを作ればすぐに
PGroongaが使える状態になっています。

## ライセンス

ライセンスはBSDライセンスやMITライセンスと類似の
[PostgreSQLライセンス](http://opensource.org/licenses/postgresql)です。

著作権保持者などの詳細は[COPYING](COPYING)ファイルを参照してください。

## TODO

  * 実装
    * WAL対応
    * COLLATE対応（今は必ずGroongaのNormalizerAutoを使っている）
  * ドキュメント
    * 英語で書く
    * サイトを用意する

## 感謝

  * 板垣さん
    * PGroongaは板垣さんが開発した[textsearch_groonga](http://textsearch-ja.projects.pgfoundry.org/textsearch_groonga.html)をベースにしています。
