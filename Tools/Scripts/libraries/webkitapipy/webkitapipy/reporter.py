from __future__ import annotations
from fnmatch import fnmatch
from . import program
from .sdkdb import SDKDB, Diagnostic, MissingName, UnusedAllowedName, UnnecessaryAllowedName, SYMBOL, OBJC_CLS, OBJC_SEL


class Reporter:
    def __init__(self, args: program.Options):
        self.print_names = len(args.input_files) > 1
        self.issues: list[Diagnostic] = []

    def emit_diagnostic(self, diag: Diagnostic) -> bool:
        ignored = False
        if isinstance(diag, MissingName):
            if diag.kind is SYMBOL:
                ignored = diag.name in program.ALLOWED_SYMBOLS
                if not ignored:
                    ignored = any(fnmatch(diag.name, pattern)
                                  for pattern in program.ALLOWED_SYMBOL_GLOBS)
            elif diag.kind is OBJC_CLS:
                ignored = f'_OBJC_CLASS_$_{diag.name}' in program.ALLOWED_SYMBOLS
        if not ignored:
            self.issues.append(diag)
            print(self.format_diagnostic(diag))
            return True
        return False

    def format_diagnostic(self, diag: Diagnostic) -> str:
        raise NotImplementedError

    def finished(self):
        pass


class TSVReporter(Reporter):
    def format_diagnostic(self, diag: Diagnostic) -> str:
        if isinstance(diag, MissingName):
            name_prefix = f'{diag.file}({diag.arch})\t' if self.print_names else ''
            return f'{name_prefix}{diag.kind}\t{diag.name}'
        elif isinstance(diag, (UnusedAllowedName, UnnecessaryAllowedName)):
            name_prefix = f'{diag.file}\t' if self.print_names else ''
            return f'{name_prefix}allowlist\t{diag.name}'


class BuildToolReporter(Reporter):
    bug_placeholder = 'https://webkit.org/b/OOPS!'

    def __init__(self, args: program.Options):
        super().__init__(args)
        self.emit_errors = args.errors
        self.suggested_allowlists = [path for path in (args.allowlists or ())
                                     if 'legacy' not in path.name]

    def format_diagnostic(self, diag: Diagnostic) -> str:
        severity = 'error' if self.emit_errors else 'warning'
        if isinstance(diag, MissingName):
            return (f'{diag.file}({diag.arch}): {severity}: unrecognized '
                    f'{diag.kind} "{diag.name}"')
        elif isinstance(diag, UnusedAllowedName):
            return (f'{diag.file}: {severity}: allowed {diag.kind} '
                    f'"{diag.name}" is not used')
        elif isinstance(diag, UnnecessaryAllowedName):
            # FIXME: exported_in is the name of the loaded file, which can be a
            # .sdkdb or .tbd that doesn't correspond to the library name on the
            # system. It would be preferable to track the install name that the
            # declaration will be implemented in, and surface that here.
            return (f'{diag.file}: {severity}: allowed {diag.kind} '
                    f'"{diag.name}" is exported from '
                    f'"{diag.exported_in.name}" and can be removed')

    def allowlist_entry(self):
        missing_names = [d for d in self.issues if isinstance(d, MissingName)]
        clss = '\n    '.join(f'"{d.name}",'
                             for d in missing_names if d.kind == OBJC_CLS)
        sels = '\n    '.join(f'"{d.name}",'
                             for d in missing_names if d.kind == OBJC_SEL)
        syms = '\n    '.join(f'"{d.name}",'
                             for d in missing_names if d.kind == SYMBOL)
        entry = f'[<category>."{self.bug_placeholder}"]'
        if clss:
            entry += f'\nclasses = [\n    {clss}\n]'
        if sels:
            entry += f'\nselectors = [\n    {sels}\n]'
        if syms:
            entry += f'\nsymbols = [\n    {syms}\n]'
        return entry

    def finished(self):
        if self.issues:
            if self.suggested_allowlists:
                allowlists = '│ \n    '.join(map(str, self.suggested_allowlists))
                allowlist_entry = self.allowlist_entry().replace('\n', '\n│     ')
                print(f'''\
│ If new SPI usage is intentional, please update one of this configuration's
│ allowlists:
│
│     {allowlists}
│
│ with the following entry:
│
│     {allowlist_entry}
│
│ Pick a <category> name based on how the SPI is being used, and file a bug
│ to track this SPI's removal.''')


def configure_reporter(args: program.Options, db: SDKDB) -> Reporter:
    cls = {
        'tsv': TSVReporter,
        'build-tool': BuildToolReporter,
    }[args.format]
    return cls(args)
