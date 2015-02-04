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

<!--  * Ubuntu 14.10 -->
  * CentOS 7

その他の環境ではソースからインストールしてください。

それぞれの環境でのインストール方法の詳細は以降のセクションで説明します。

<!--

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

-->

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

    % wget http://packages.groonga.org/source/pgroonga/pgroonga-0.2.0.tar.gz
    % tar xvf pgroonga-0.2.0.tar.gz
    % cd pgroonga-0.2.0

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
```

検索します。ここではシーケンシャルスキャンではなくインデックスを使った
全文検索を使いたいので、シーケンシャルスキャン機能を無効にします。（あ
るいはもっとたくさんのデータを投入します。）

```sql
SET enable_seqscan = off;
```

全文検索をするときは`%%`演算子を使います。

```sql
SELECT * FROM memos WHERE content %% '全文検索';
--  id |                      content
-- ----+---------------------------------------------------
--   2 | Groongaは日本語対応の高速な全文検索エンジンです。
-- (1 行)
```

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

#### カスタマイズ

TODO:

  * トークナイザーのカスタマイズ方法を書く
  * ノーマライザーのカスタマイズ方法を書く

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

文字列型のときは`USING`に`pgroonga`を指定するだけでなく、演算子クラス
として`pgroonga.text_ops`を指定してください。

```sql
CREATE TABLE tags (
  id integer,
  tag text
);

CREATE INDEX pgroonga_tag_index ON tags USING pgroonga (tag pgroonga.text_ops);
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

## アンインストール

次のSQLでアンインストールできます。

```sql
DROP EXTENSION pgroonga CASCADE;
DELETE FROM pg_catalog.pg_am WHERE amname = 'pgroonga';
```

`pg_catalog.pg_am`から手動でレコードを消さないといけないのはおかしい気
がするので、何がおかしいか知っている人は教えてくれるとうれしいです。

## ライセンス

ライセンスはBSDライセンスやMITライセンスと類似の
[PostgreSQLライセンス](http://opensource.org/licenses/postgresql)です。

著作権保持者などの詳細は[COPYING](COPYING)ファイルを参照してください。

## TODO

  * 実装
    * WAL対応
    * スコアー対応
    * COLLATE対応（今は必ずGroongaのNormalizerAutoを使っている）
  * ドキュメント
    * 英語で書く
    * サイトを用意する

## 感謝

  * 板垣さん
    * PGroongaは板垣さんが開発した[textsearch_groonga](http://textsearch-ja.projects.pgfoundry.org/textsearch_groonga.html)をベースにしています。
