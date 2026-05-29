# Usage:
```
cc track.c -o track -Wall -Wextra
./track
```

# Commands
`track create <PRIORITY> <DESCRIPTION> <TAGS>` - creates new task in nearest `tasks` folder with given priority, description and tags. For multiple tags use `,` as delimiter.\
`track ls <OPTIONS>` - shows all available tasks. Available options: `--no-color` and `--filter <STRING>`.\
`track init` - creates new `tasks` directory in current directory.\
`track label <DIRPATH>` - either creates `.timelabel` file for given task or updates existing one. Appends unixtime timestamp into `.timelabel` file.\
`track open <DIRPATH>` - sets status `OPEN` to given task.\
`track close <DIRPATH>` - sets status `CLOSED` to given task.\

# Filter
Filter string consists of: `<<field> <operator> <value>>`. Whitespaces are mandatory.\
Possible fields: `status`, `tags`, `priority`.\
Possible operators: `<`, `>`, `<=`, `>=`, `=`.\
Possible values:
- `CLOSED` or `OPEN` if field equals to `status`
- Any integer value if field equals to `priority`
- Any tag if field equals to `tags`.
### Example: 
```
track ls --filter "status = CLOSED"
```
Multiple filters with different fields can be chained. Example:
```
track ls --filter "status = CLOSED" --filter "priority > 90" --filter "tags = bug, asap"
```
