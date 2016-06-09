# Completion example

## How to try

Run the following command to setup data:

```text
% ./setup.sh | psql -d DB
```

Search by the following command:

```text
% ./complete.sh nak | psql -d DB
% ./complete.sh なか | psql -d DB
% ./complete.sh 中 | psql -d DB
```
