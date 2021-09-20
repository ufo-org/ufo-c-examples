# ufo-c-examples

## Postgresql example

```bash
sudo apt install postgresql postgresql-contrib
sudo -i -u postgres
```

```bash
createuser -s "my_user" # set to whatever user will be running the queries
# alternative: createuser --interactive
```

```bash
createdb ufo
psql -d ufo
```

See `team.sql` for table and example data.

```bash
make postgres
```

```bash
./postgres
```


## Troubleshooting

```
thread '<unnamed>' panicked at 'called `Result::unwrap()` on an `Err` value: SystemError(Sys(EPERM))', .../src/ufo_core.rs:215:14
```

System is set up only to allow root use `userfaultf`. Fix by setting system
priivileges to allow unprivileged users to use `userfaultfd`:

```bash
sysctl -w vm.unprivileged_userfaultfd=1
```
