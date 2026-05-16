# CLI Mode Usage

EasyKiConverter supports command-line mode, allowing conversion operations without opening the GUI.

## Subcommands

```bash
# Convert BOM file
easykiconverter convert bom -i <bom_file> -o <output_dir> [options]

# Convert single component
easykiconverter convert component -c <lcsc_id> -o <output_dir> [options]

# Batch conversion
easykiconverter convert batch -i <component_list_file> -o <output_dir> [options]
```

## Common Arguments

| Argument | Short | Description | Default |
|----------|-------|-------------|---------|
| `--input` | `-i` | Input file path | - |
| `--output` | `-o` | Output directory path | - |
| `--component` | `-c` | LCSC component ID | - |
| `--symbol` | | Export symbol library | true |
| `--footprint` | | Export footprint library | true |
| `--3d-model` | | Export 3D models (default WRL format) | false |
| `--preview` | | Export preview images | false |
| `--cache-dir` | | Set disk cache directory | Cache directory from config |
| `--cache-size-mb` | | Set disk cache size limit (MB) | Cache size limit from config |
| `--progress` | | Show progress bar | false |
| `--quiet` | `-q` | Quiet mode | false |
| `--debug` | `-d` | Debug mode (generate detailed reports) | false |
| `--completion` | | Generate shell completion script | - |

## Default Export Options

CLI mode exports the following by default:
- Symbol library (Symbol)
- Footprint library (Footprint)

**Note**:
- 3D models, preview images, and datasheets are not exported by default
- Use `--3d-model` when needed; defaults to WRL format if not specified
- Normal mode does not generate detailed reports; only in debug mode (`--debug`)

## Cache Configuration

CLI mode reuses the same component disk cache as GUI. You can override the cache directory and size limit for the current run:

```bash
easykiconverter convert component -c C12345 -o ./output \
  --cache-dir /tmp/easykiconverter-cache \
  --cache-size-mb 2048
```

Notes:
- `--cache-dir` specifies the cache root directory for this run
- Valid range for `--cache-size-mb` is `1` to `1048576`
- When modifying the cache directory in GUI, the app attempts to migrate old cache data to the new directory to reuse already-downloaded data; existing files with the same name are not overwritten
- 3D model cache is stored in the `model3d` subdirectory of the cache directory, excluded from LRU eviction by default to avoid frequent re-downloading of large files

## Shell Completion

EasyKiConverter supports shell completion for bash, zsh, and fish.

### Bash Completion

```bash
# Temporary (current session)
eval "$(easykiconverter --completion bash)"

# Permanent
easykiconverter --completion bash >> ~/.bash_completion
echo 'source ~/.bash_completion' >> ~/.bashrc
```

### Zsh Completion

```bash
# Temporary (current session)
eval "$(easykiconverter --completion zsh)"

# Permanent
mkdir -p ~/.zsh
easykiconverter --completion zsh > ~/.zsh/_easykiconverter
echo 'fpath=(~/.zsh $fpath)' >> ~/.zshrc
echo 'autoload -Uz compinit && compinit' >> ~/.zshrc
```

### Fish Completion

```bash
# Permanent
easykiconverter --completion fish > ~/.config/fish/completions/easykiconverter.fish
```

### Completion Features

- Argument option completion: auto-completes `--help`, `--input`, `--output`, etc.
- Subcommand completion: auto-completes `convert`, `bom`, `component`, `batch`, etc.
- File completion: after entering `-i`, auto-completes `.xlsx`, `.csv`, etc.
- Directory completion: after entering `-o`, auto-completes directory paths
- **Dynamic LCSC ID completion**: after entering `-c`, auto-completes cached component IDs

## Usage Examples

```bash
# Default export (symbol + footprint library)
easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs

# Export symbol, footprint, and 3D models (default WRL format)
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model

# Export everything including preview images
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --preview

# Debug mode (generate detailed report)
easykiconverter convert bom -i my_project.xlsx -o ./output --debug

# With progress bar
easykiconverter convert bom -i bom.xlsx -o ./output --progress

# Custom cache directory with 2GB disk cache limit
easykiconverter convert component -c C12345 -o ./output --cache-dir /tmp/ekc-cache --cache-size-mb 2048

# Convert single component
easykiconverter convert component -c C12345 -o ./output

# Batch conversion
easykiconverter convert batch -i components.txt -o ./output
```

## Architecture

The CLI module uses a loosely coupled design with the following components:

- `CommandLineParser` - Command-line argument parser
- `CliContext` - CLI context holding shared state
- `CliPrinter` - CLI output utility
- `FileReader` - File reading utility
- `BaseConverter` - Base converter class
- `BomConverter` - BOM file converter
- `ComponentConverter` - Single component converter
- `BatchConverter` - Batch converter
- `CliConverter` - Main coordinator
- `CompletionGenerator` - Shell completion script generator