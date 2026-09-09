"""Keep prose and return dereferences out of C++ ownership diagnostics."""

import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("boundaries", Path(__file__).with_name("check-boundaries.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

for source in ('"new action after delete rejected"', 'R"tag(new Thing; delete thing)tag"',
               '// new Thing\n', '/* delete thing */', 'auto x = "https://x/new Thing";',
               'new_snapshot = newer; delete_count = 0;'):
    assert not module.MANUAL_OWNERSHIP.search(module.code_tokens(source)), source
for source in ('new Thing', 'delete thing;', 'delete[] thing;', '"https://x"; new Thing',
               'R"x(new Thing)x"; delete thing;'):
    assert module.MANUAL_OWNERSHIP.search(module.code_tokens(source)), source
assert not module.RAW_POINTER.search(module.code_tokens('return *current_;'))
for source in ('Thing* member_;', 'const Thing* Value();', 'std::string const * observer_;'):
    assert module.RAW_POINTER.search(module.code_tokens(source)), source
assert module.without_comments('"https://example" // note') == '"https://example"  '
print("Boundary lexer keeps actual ownership violations and ignores literals/dereferences")
