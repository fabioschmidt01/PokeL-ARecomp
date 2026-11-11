using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using LibHac.Common.Keys;

namespace PokeLExtractor;

public partial class MainForm : Form
{
    private TextBox txtInput = null!;
    private Button btnBrowseInput = null!;
    private CheckBox chkRecursive = null!;

    private TextBox txtKeys = null!;
    private Button btnBrowseKeys = null!;
    private Label lblKeysStatus = null!;

    private TextBox txtOutput = null!;
    private Button btnBrowseOutput = null!;
    private Button btnUseRecompOutput = null!;

    private Button btnScan = null!;
    private ListView listPackages = null!;
    private ImageList iconList = null!;

    private ProgressBar progressBar = null!;
    private Label lblProgress = null!;
    private Button btnExtract = null!;
    private Button btnCancel = null!;
    private Button btnOpenOutput = null!;

    private RichTextBox txtLog = null!;

    private List<SwitchPackageInfo> detectedPackages = new();
    private KeySet? currentKeySet;
    private CancellationTokenSource? extractionCts;
    private readonly ExtractorEngine engine = new();

    public MainForm()
    {
        InitializeComponent();
        BuildUI();
        RegisterEvents();

        // Default paths based on workspace
        string defaultGameDir = @"D:\Rons\Pokemon Legends Arceus\jogo";
        if (Directory.Exists(defaultGameDir)) txtInput.Text = defaultGameDir;

        string defaultOutDir = @"D:\Rons\Pokemon Legends Arceus\PokeL-ARecomp-Release\PokeL-ARecomp\extracted";
        txtOutput.Text = defaultOutDir;

        if (!string.IsNullOrEmpty(txtInput.Text))
        {
            AutoDetectKeys(txtInput.Text);
            _ = ScanPackagesAsync();
        }
    }

    private void BuildUI()
    {
        this.Font = new Font("Segoe UI", 9F, FontStyle.Regular, GraphicsUnit.Point);
        this.BackColor = Color.FromArgb(245, 247, 250);
        this.AllowDrop = true;

        try
        {
            if (File.Exists("icon.ico")) this.Icon = new Icon("icon.ico");
        }
        catch { }

        var mainPanel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 5,
            Padding = new Padding(12),
        };
        mainPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 135F)); // Top configuration
        mainPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 35F));  // Scan & count bar
        mainPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 45F));   // List of packages
        mainPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 65F));  // Progress bar & extract buttons
        mainPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 55F));   // Log console

        // 1. Top Configuration Group
        var grpConfig = new GroupBox
        {
            Text = "Configurações de Origem, Chaves e Destino",
            Dock = DockStyle.Fill,
            Font = new Font("Segoe UI", 9F, FontStyle.Bold)
        };

        var configTable = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 4,
            RowCount = 3,
            Padding = new Padding(8, 4, 8, 4)
        };
        configTable.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 110F));
        configTable.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        configTable.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100F));
        configTable.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 160F));

        // Row 0: Input Directory
        var lblInput = new Label { Text = "Pasta de Origem:", Anchor = AnchorStyles.Left, AutoSize = true, Font = new Font("Segoe UI", 9F) };
        txtInput = new TextBox { Dock = DockStyle.Fill, Font = new Font("Segoe UI", 9F) };
        btnBrowseInput = new Button { Text = "Procurar...", Dock = DockStyle.Fill, Font = new Font("Segoe UI", 9F) };
        chkRecursive = new CheckBox { Text = "Incluir Subpastas", Checked = true, Anchor = AnchorStyles.Left, AutoSize = true, Font = new Font("Segoe UI", 9F) };

        configTable.Controls.Add(lblInput, 0, 0);
        configTable.Controls.Add(txtInput, 1, 0);
        configTable.Controls.Add(btnBrowseInput, 2, 0);
        configTable.Controls.Add(chkRecursive, 3, 0);

        // Row 1: Keys
        var lblKeys = new Label { Text = "Chaves Switch:", Anchor = AnchorStyles.Left, AutoSize = true, Font = new Font("Segoe UI", 9F) };
        txtKeys = new TextBox { Dock = DockStyle.Fill, Font = new Font("Segoe UI", 9F) };
        btnBrowseKeys = new Button { Text = "Procurar...", Dock = DockStyle.Fill, Font = new Font("Segoe UI", 9F) };
        lblKeysStatus = new Label { Text = "Chaves: Não carregadas", Anchor = AnchorStyles.Left, AutoSize = true, ForeColor = Color.DarkOrange, Font = new Font("Segoe UI", 8.5F, FontStyle.Italic) };

        configTable.Controls.Add(lblKeys, 0, 1);
        configTable.Controls.Add(txtKeys, 1, 1);
        configTable.Controls.Add(btnBrowseKeys, 2, 1);
        configTable.Controls.Add(lblKeysStatus, 3, 1);

        // Row 2: Output Directory
        var lblOutput = new Label { Text = "Pasta de Destino:", Anchor = AnchorStyles.Left, AutoSize = true, Font = new Font("Segoe UI", 9F) };
        txtOutput = new TextBox { Dock = DockStyle.Fill, Font = new Font("Segoe UI", 9F) };
        btnBrowseOutput = new Button { Text = "Procurar...", Dock = DockStyle.Fill, Font = new Font("Segoe UI", 9F) };
        btnUseRecompOutput = new Button { Text = "Usar Pasta Recomp", Dock = DockStyle.Fill, Font = new Font("Segoe UI", 8.5F) };

        configTable.Controls.Add(lblOutput, 0, 2);
        configTable.Controls.Add(txtOutput, 1, 2);
        configTable.Controls.Add(btnBrowseOutput, 2, 2);
        configTable.Controls.Add(btnUseRecompOutput, 3, 2);

        grpConfig.Controls.Add(configTable);
        mainPanel.Controls.Add(grpConfig, 0, 0);

        // 2. Scan Header Bar
        var scanBar = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
        btnScan = new Button { Text = "🔍 Escanear Pasta Agora", Width = 180, Height = 28, Font = new Font("Segoe UI", 9F, FontStyle.Bold) };
        var lblScanHelp = new Label { Text = "Você pode arrastar e soltar pastas ou arquivos .nsp/.xci diretamente nesta janela.", AutoSize = true, Padding = new Padding(10, 6, 0, 0), ForeColor = Color.Gray };
        scanBar.Controls.Add(btnScan);
        scanBar.Controls.Add(lblScanHelp);
        mainPanel.Controls.Add(scanBar, 0, 1);

        // 3. Package ListView
        iconList = new ImageList { ImageSize = new Size(36, 36), ColorDepth = ColorDepth.Depth32Bit };

        listPackages = new ListView
        {
            Dock = DockStyle.Fill,
            View = View.Details,
            CheckBoxes = true,
            FullRowSelect = true,
            GridLines = true,
            SmallImageList = iconList,
            Font = new Font("Segoe UI", 9F)
        };
        listPackages.Columns.Add("Jogo / Arquivo", 320);
        listPackages.Columns.Add("Classificação", 160);
        listPackages.Columns.Add("Title ID", 150);
        listPackages.Columns.Add("Versão", 80);
        listPackages.Columns.Add("Tamanho", 90);
        listPackages.Columns.Add("Caminho Completo", 300);

        mainPanel.Controls.Add(listPackages, 0, 2);

        // 4. Progress and Action Buttons
        var progressPanel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 4,
            RowCount = 2
        };
        progressPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        progressPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 160F));
        progressPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 110F));
        progressPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150F));

        progressBar = new ProgressBar { Dock = DockStyle.Fill, Height = 24 };
        lblProgress = new Label { Text = "Pronto para iniciar.", AutoSize = true, Anchor = AnchorStyles.Left, ForeColor = Color.FromArgb(50, 50, 50) };

        btnExtract = new Button
        {
            Text = "▶ Iniciar Extração",
            Dock = DockStyle.Fill,
            Height = 45,
            Font = new Font("Segoe UI", 10F, FontStyle.Bold),
            BackColor = Color.FromArgb(46, 125, 50),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        btnExtract.FlatAppearance.BorderSize = 0;

        btnCancel = new Button
        {
            Text = "⏹ Cancelar",
            Dock = DockStyle.Fill,
            Height = 45,
            Enabled = false,
            Font = new Font("Segoe UI", 9F)
        };

        btnOpenOutput = new Button
        {
            Text = "📂 Abrir Destino",
            Dock = DockStyle.Fill,
            Height = 45,
            Font = new Font("Segoe UI", 9F)
        };

        progressPanel.Controls.Add(progressBar, 0, 0);
        progressPanel.Controls.Add(btnExtract, 1, 0);
        progressPanel.SetRowSpan(btnExtract, 2);
        progressPanel.Controls.Add(btnCancel, 2, 0);
        progressPanel.SetRowSpan(btnCancel, 2);
        progressPanel.Controls.Add(btnOpenOutput, 3, 0);
        progressPanel.SetRowSpan(btnOpenOutput, 2);

        progressPanel.Controls.Add(lblProgress, 0, 1);
        mainPanel.Controls.Add(progressPanel, 0, 3);

        // 5. RichTextBox Log Console
        var grpLog = new GroupBox
        {
            Text = "Console de Log em Tempo Real",
            Dock = DockStyle.Fill,
            Font = new Font("Segoe UI", 8.5F, FontStyle.Bold)
        };
        txtLog = new RichTextBox
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            BackColor = Color.FromArgb(28, 30, 36),
            ForeColor = Color.FromArgb(220, 220, 220),
            Font = new Font("Consolas", 9F),
            BorderStyle = BorderStyle.None
        };
        grpLog.Controls.Add(txtLog);
        mainPanel.Controls.Add(grpLog, 0, 4);

        this.Controls.Add(mainPanel);
    }

    private void RegisterEvents()
    {
        btnBrowseInput.Click += (s, e) =>
        {
            using var fbd = new FolderBrowserDialog { Description = "Selecione a pasta onde estão seus arquivos .nsp / .xci" };
            if (fbd.ShowDialog() == DialogResult.OK)
            {
                txtInput.Text = fbd.SelectedPath;
                AutoDetectKeys(txtInput.Text);
                _ = ScanPackagesAsync();
            }
        };

        btnBrowseKeys.Click += (s, e) =>
        {
            using var ofd = new OpenFileDialog
            {
                Title = "Selecione seu prod.keys",
                Filter = "Chaves Switch (*.keys;*.key;*.txt)|*.keys;*.key;*.txt|Todos os Arquivos (*.*)|*.*"
            };
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                txtKeys.Text = ofd.FileName;
                LoadKeySet(txtKeys.Text);
                _ = ScanPackagesAsync();
            }
        };

        btnBrowseOutput.Click += (s, e) =>
        {
            using var fbd = new FolderBrowserDialog { Description = "Selecione a pasta onde o jogo extraído será salvo" };
            if (fbd.ShowDialog() == DialogResult.OK) txtOutput.Text = fbd.SelectedPath;
        };

        btnUseRecompOutput.Click += (s, e) =>
        {
            string recDir = @"D:\Rons\Pokemon Legends Arceus\PokeL-ARecomp-Release\PokeL-ARecomp\extracted";
            txtOutput.Text = recDir;
            AppendLog(LogType.Info, $"Destino definido para a pasta do PokeL-ARecomp: {recDir}");
        };

        btnScan.Click += async (s, e) => await ScanPackagesAsync();

        chkRecursive.CheckedChanged += async (s, e) => await ScanPackagesAsync();

        btnOpenOutput.Click += (s, e) =>
        {
            if (Directory.Exists(txtOutput.Text))
                Process.Start(new ProcessStartInfo { FileName = txtOutput.Text, UseShellExecute = true });
            else
                MessageBox.Show("A pasta de destino ainda não existe.", "Aviso", MessageBoxButtons.OK, MessageBoxIcon.Information);
        };

        this.DragEnter += (s, e) =>
        {
            if (e.Data != null && e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.Copy;
        };

        this.DragDrop += (s, e) =>
        {
            if (e.Data != null && e.Data.GetData(DataFormats.FileDrop) is string[] files && files.Length > 0)
            {
                string first = files[0];
                if (Directory.Exists(first))
                    txtInput.Text = first;
                else
                    txtInput.Text = Path.GetDirectoryName(first) ?? first;

                AutoDetectKeys(txtInput.Text);
                _ = ScanPackagesAsync();
            }
        };

        btnExtract.Click += async (s, e) => await StartExtractionAsync();

        btnCancel.Click += (s, e) =>
        {
            extractionCts?.Cancel();
            btnCancel.Enabled = false;
        };

        engine.OnLog += (level, msg) =>
        {
            if (InvokeRequired) Invoke(new Action(() => AppendLog(level, msg)));
            else AppendLog(level, msg);
        };

        engine.OnProgress += (info) =>
        {
            if (InvokeRequired)
            {
                Invoke(new Action(() => UpdateProgress(info)));
            }
            else
            {
                UpdateProgress(info);
            }
        };
    }

    private void UpdateProgress(ProgressInfo info)
    {
        progressBar.Value = (int)Math.Clamp(info.Percentage, 0, 100);
        string doneStr = SwitchPackageInfo.FormatBytes(info.BytesDone);
        string totalStr = SwitchPackageInfo.FormatBytes(info.TotalBytes);
        string etaStr = info.Eta > TimeSpan.Zero ? $"Restante: {info.Eta:mm\\:ss}" : "";

        lblProgress.Text = $"{info.Operation} — {doneStr} / {totalStr} ({info.Percentage:F1}%)  [{info.SpeedMBs:F1} MB/s]  {etaStr}";
    }

    private void AutoDetectKeys(string searchDir)
    {
        var (prod, title) = PackageScanner.FindKeysFiles(searchDir, chkRecursive.Checked);
        if (prod == null && Directory.Exists(@"D:\Rons\Pokemon Legends Arceus\jogo\ProdKeys"))
        {
            prod = @"D:\Rons\Pokemon Legends Arceus\jogo\ProdKeys\prod.keys";
        }

        if (prod != null)
        {
            txtKeys.Text = prod;
            LoadKeySet(prod, title);
        }
        else
        {
            lblKeysStatus.Text = "Chaves: Não encontradas";
            lblKeysStatus.ForeColor = Color.Red;
        }
    }

    private void LoadKeySet(string prodPath, string? titlePath = null)
    {
        try
        {
            currentKeySet = PackageScanner.LoadKeySet(prodPath, titlePath);
            if (!currentKeySet.HeaderKey.IsZeros())
            {
                lblKeysStatus.Text = "Chaves: OK (header_key válida)";
                lblKeysStatus.ForeColor = Color.Green;
                AppendLog(LogType.Success, $"Chaves carregadas com sucesso de: {prodPath}");
            }
            else
            {
                lblKeysStatus.Text = "Chaves: Sem header_key!";
                lblKeysStatus.ForeColor = Color.Red;
                AppendLog(LogType.Error, "O prod.keys selecionado não possui header_key.");
            }
        }
        catch (Exception ex)
        {
            lblKeysStatus.Text = "Erro ao carregar chaves";
            lblKeysStatus.ForeColor = Color.Red;
            AppendLog(LogType.Error, $"Falha ao ler chaves: {ex.Message}");
        }
    }

    private async Task ScanPackagesAsync()
    {
        string dir = txtInput.Text.Trim();
        if (!Directory.Exists(dir))
        {
            AppendLog(LogType.Warning, "A pasta de origem especificada não existe.");
            return;
        }

        btnScan.Enabled = false;
        listPackages.Items.Clear();
        detectedPackages.Clear();
        iconList.Images.Clear();

        AppendLog(LogType.Info, $"Escaneando pasta: {dir} (Subpastas: {(chkRecursive.Checked ? "Sim" : "Não")})...");

        if (currentKeySet == null && !string.IsNullOrEmpty(txtKeys.Text))
            LoadKeySet(txtKeys.Text);

        var files = await Task.Run(() => PackageScanner.FindPackageFiles(dir, chkRecursive.Checked));
        AppendLog(LogType.Info, $"Arquivos encontrados: {files.Count} (.nsp / .xci)");

        if (currentKeySet == null)
        {
            AppendLog(LogType.Warning, "Nenhum prod.keys carregado. A identificação detalhada requer prod.keys.");
        }

        int index = 0;
        foreach (var file in files)
        {
            var info = await Task.Run(() => PackageScanner.InspectPackage(file, currentKeySet ?? new KeySet()));
            if (info != null)
            {
                detectedPackages.Add(info);

                if (info.Icon != null)
                {
                    iconList.Images.Add(info.TitleIdHex, info.Icon);
                }

                var item = new ListViewItem(new[]
                {
                    $"{info.GameTitle} ({info.FileName})",
                    info.TypeDescription,
                    info.TitleIdHex,
                    !string.IsNullOrEmpty(info.DisplayVersion) ? info.DisplayVersion : "-",
                    info.DisplaySize,
                    info.FilePath
                })
                {
                    Checked = true,
                    Tag = info,
                    ImageKey = info.Icon != null ? info.TitleIdHex : ""
                };

                listPackages.Items.Add(item);
                AppendLog(LogType.Success, $"Identificado: {info.GameTitle} -> {info.TypeDescription} [TitleId: {info.TitleIdHex}] ({info.DisplaySize})");
                index++;
            }
        }

        btnScan.Enabled = true;
        lblProgress.Text = $"{detectedPackages.Count} pacotes prontos para extração.";
    }

    private async Task StartExtractionAsync()
    {
        if (currentKeySet == null || currentKeySet.HeaderKey.IsZeros())
        {
            MessageBox.Show("Você precisa fornecer um prod.keys válido com header_key para descriptografar os arquivos.", "Chaves Ausentes", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        string outDir = txtOutput.Text.Trim();
        if (string.IsNullOrEmpty(outDir))
        {
            MessageBox.Show("Selecione uma pasta de destino.", "Destino Inválido", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        // Update selected flags from list
        foreach (ListViewItem item in listPackages.Items)
        {
            if (item.Tag is SwitchPackageInfo info)
                info.IsSelected = item.Checked;
        }

        var toExtract = detectedPackages.Where(p => p.IsSelected).ToList();
        if (toExtract.Count == 0)
        {
            MessageBox.Show("Marque pelo menos um pacote para extrair.", "Nenhum Item Selecionado", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        btnExtract.Enabled = false;
        btnScan.Enabled = false;
        btnCancel.Enabled = true;
        extractionCts = new CancellationTokenSource();

        try
        {
            bool ok = await engine.ExtractAsync(currentKeySet, outDir, toExtract, extractionCts.Token);
            if (ok)
            {
                MessageBox.Show("Extração e fusão concluídas com sucesso!\n\nOs arquivos estão prontos para o PokeL-ARecomp.", "Sucesso", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }
        catch (Exception ex)
        {
            AppendLog(LogType.Error, $"Erro fatal: {ex.Message}");
        }
        finally
        {
            btnExtract.Enabled = true;
            btnScan.Enabled = true;
            btnCancel.Enabled = false;
            extractionCts = null;
        }
    }

    private void AppendLog(LogType type, string text)
    {
        Color color = type switch
        {
            LogType.Success => Color.FromArgb(76, 175, 80),
            LogType.Warning => Color.FromArgb(255, 193, 7),
            LogType.Error => Color.FromArgb(244, 67, 54),
            _ => Color.FromArgb(100, 181, 246)
        };

        txtLog.SelectionStart = txtLog.TextLength;
        txtLog.SelectionLength = 0;
        txtLog.SelectionColor = Color.FromArgb(140, 140, 140);
        txtLog.AppendText($"[{DateTime.Now:HH:mm:ss}] ");

        txtLog.SelectionColor = color;
        txtLog.AppendText($"{text}\n");
        txtLog.ScrollToCaret();
    }
}
