; *** Inno Setup version 6.5.0+ Portuguese (Portugal) messages ***
;
; TranslatorNote=Portuguese (Portugal) translation by Nuno Silva (nars@gmx.net) and BlackSpirits (blackspirits@gmail.com)
; Last modified: 22 November 2025 by BlackSpirits

[LangOptions]
LanguageName=Português (Portugal)
LanguageID=$0816
LanguageCodePage=1252

[Messages]

; *** Application titles
SetupAppTitle=Instalação
SetupWindowTitle=%1 - Instalação
UninstallAppTitle=Desinstalação
UninstallAppFullTitle=%1 - Desinstalação

; *** Misc. common
InformationTitle=Informação
ConfirmTitle=Confirmação
ErrorTitle=Erro

; *** SetupLdr messages
SetupLdrStartupMessage=Irá ser instalado o %1. Deseja continuar?
LdrCannotCreateTemp=Não foi possível criar um ficheiro temporário. Instalação cancelada
LdrCannotExecTemp=Não foi possível executar um ficheiro na pasta temporária. Instalação cancelada
HelpTextNote=

; *** Startup error messages
LastErrorMessage=%1.%n%nErro %2: %3
SetupFileMissing=O ficheiro %1 não foi encontrado na pasta de instalação. Corrija o problema ou obtenha uma nova cópia do programa.
SetupFileCorrupt=Os ficheiros de instalação estão corrompidos. Obtenha uma nova cópia do programa.
SetupFileCorruptOrWrongVer=Os ficheiros de instalação estão corrompidos, ou são incompatíveis com esta versão do Assistente de Instalação. Corrija o problema ou obtenha uma nova cópia do programa.
InvalidParameter=Foi especificado um parâmetro inválido na linha de comando:%n%n%1
SetupAlreadyRunning=A instalação já está em execução.
WindowsVersionNotSupported=Este programa não suporta a versão do Windows que está a utilizar.
WindowsServicePackRequired=Este programa necessita de %1 Service Pack %2 ou mais recente.
NotOnThisPlatform=Este programa não pode ser executado no %1.
OnlyOnThisPlatform=Este programa deve ser executado no %1.
OnlyOnTheseArchitectures=Este programa só pode ser instalado em versões do Windows preparadas para as seguintes arquiteturas:%n%n%1
WinVersionTooLowError=Este programa necessita do %1 versão %2 ou mais recente.
WinVersionTooHighError=Este programa não pode ser instalado no %1 versão %2 ou mais recente.
AdminPrivilegesRequired=Deve iniciar sessão como administrador para instalar este programa.
PowerUserPrivilegesRequired=Deve iniciar sessão como administrador ou membro do grupo de Utilizadores Avançados para instalar este programa.
SetupAppRunningError=O Assistente de Instalação detetou que o %1 está em execução. Feche-o e de seguida clique em OK para continuar, ou clique em Cancelar para cancelar a instalação.
UninstallAppRunningError=O Assistente de Desinstalação detetou que o %1 está em execução. Feche-o e de seguida clique em OK para continuar, ou clique em Cancelar para cancelar a desinstalação.

; *** Startup questions
PrivilegesRequiredOverrideTitle=Selecione o Modo de Instalação
PrivilegesRequiredOverrideInstruction=Selecione o Modo de Instalação
PrivilegesRequiredOverrideText1=%1 pode ser instalado para todos os utilizadores (necessita de privilégios administrativos), ou só para si.
PrivilegesRequiredOverrideText2=%1 pode ser instalado só para si, ou para todos os utilizadores (necessita de privilégios administrativos).
PrivilegesRequiredOverrideAllUsers=Instalar para &todos os utilizadores
PrivilegesRequiredOverrideAllUsersRecommended=Instalar para &todos os utilizadores (recomendado)
PrivilegesRequiredOverrideCurrentUser=Instalar apenas para &mim
PrivilegesRequiredOverrideCurrentUserRecommended=Instalar apenas para &mim (recomendado)

; *** Misc. errors
ErrorCreatingDir=O Assistente de Instalação não consegue criar a pasta "%1"
ErrorTooManyFilesInDir=Não é possível criar um ficheiro na pasta "%1" porque esta contém demasiados ficheiros

; *** Setup common messages
ExitSetupTitle=Terminar a instalação
ExitSetupMessage=A instalação não está completa. Se terminar agora, o programa não será instalado.%n%nMais tarde poderá executar novamente este Assistente de Instalação e concluir a instalação.%n%nDeseja terminar a instalação?
AboutSetupMenuItem=&Acerca de...
AboutSetupTitle=Acerca do Assistente de Instalação
AboutSetupMessage=%1 versão %2%n%3%n%n%1 home page:%n%4
AboutSetupNote=
TranslatorNote=Original PT-PT translation by NARS. Revised and fully updated to AO90 by BlackSpirits

; *** Buttons
ButtonBack=< &Anterior
ButtonNext=&Seguinte >
ButtonInstall=&Instalar
ButtonOK=OK
ButtonCancel=Cancelar
ButtonYes=&Sim
ButtonYesToAll=Sim para &todos
ButtonNo=&Não
ButtonNoToAll=Nã&o para todos
ButtonFinish=&Concluir
ButtonBrowse=&Procurar...
ButtonWizardBrowse=P&rocurar...
ButtonNewFolder=&Criar Nova Pasta

; *** "Select Language" dialog messages
SelectLanguageTitle=Selecione o Idioma do Assistente de Instalação
SelectLanguageLabel=Selecione o idioma para usar durante a Instalação.

; *** Common wizard text
ClickNext=Clique em Seguinte para continuar ou em Cancelar para cancelar a instalação.
BeveledLabel=
BrowseDialogTitle=Procurar Pasta
BrowseDialogLabel=Selecione uma pasta na lista abaixo e clique em OK.
NewFolderName=Nova Pasta

; *** "Welcome" wizard page
WelcomeLabel1=Bem-vindo ao Assistente de Instalação do [name]
WelcomeLabel2=O Assistente de Instalação irá instalar o [name/ver] no seu computador.%n%nÉ recomendado que feche todas as outras aplicações antes de continuar.

; *** "Password" wizard page
WizardPassword=Palavra-passe
PasswordLabel1=Esta instalação está protegida por palavra-passe.
PasswordLabel3=Insira a palavra-passe e de seguida clique em Seguinte para continuar. Na palavra-passe existe diferença entre maiúsculas e minúsculas.
PasswordEditLabel=&Palavra-passe:
IncorrectPassword=A palavra-passe que introduziu não está correta. Tente novamente.

; *** "License Agreement" wizard page
WizardLicense=Contrato de licença
LicenseLabel=É importante que leia as seguintes informações antes de continuar.
LicenseLabel3=Leia atentamente o seguinte contrato de licença. Deve aceitar os termos do contrato antes de continuar a instalação.
LicenseAccepted=A&ceito o contrato
LicenseNotAccepted=&Não aceito o contrato

; *** "Information" wizard pages
WizardInfoBefore=Informação
InfoBeforeLabel=É importante que leia as seguintes informações antes de continuar.
InfoBeforeClickLabel=Quando estiver pronto para continuar clique em Seguinte.
WizardInfoAfter=Informação
InfoAfterLabel=É importante que leia as seguintes informações antes de continuar.
InfoAfterClickLabel=Quando estiver pronto para continuar clique em Seguinte.

; *** "User Information" wizard page
WizardUserInfo=Informações do utilizador
UserInfoDesc=Introduza as suas informações.
UserInfoName=Nome do &utilizador:
UserInfoOrg=&Organização:
UserInfoSerial=&Número de série:
UserInfoNameRequired=Deve introduzir um nome.

; *** "Select Destination Location" wizard page
WizardSelectDir=Selecione a localização de destino
SelectDirDesc=Onde deverá ser instalado o [name]?
SelectDirLabel3=O [name] será instalado na seguinte pasta.
SelectDirBrowseLabel=Para continuar, clique em Seguinte. Se desejar selecionar uma pasta diferente, clique em Procurar.
DiskSpaceGBLabel=É necessário pelo menos [gb] GB de espaço livre em disco.
DiskSpaceMBLabel=É necessário pelo menos [mb] MB de espaço livre em disco.
CannotInstallToNetworkDrive=O Assistente de Instalação não pode instalar numa unidade de rede.
CannotInstallToUNCPath=O Assistente de Instalação não pode instalar num caminho UNC.
InvalidPath=É necessário indicar o caminho completo com a letra de unidade; por exemplo:%n%nC:\APP%n%nou um caminho UNC no formato:%n%n\\servidor\partilha
InvalidDrive=A unidade ou partilha UNC selecionada não existe ou não está acessível. Selecione outra.
DiskSpaceWarningTitle=Não há espaço suficiente no disco
DiskSpaceWarning=O Assistente de Instalação necessita de pelo menos %1 KB de espaço livre, mas a unidade selecionada tem apenas %2 KB disponíveis.%n%nDeseja continuar de qualquer forma?
DirNameTooLong=O nome ou caminho para a pasta é demasiado longo.
InvalidDirName=O nome da pasta não é válido.
BadDirName32=O nome da pasta não pode conter nenhum dos seguintes caracteres:%n%n%1
DirExistsTitle=A pasta já existe
DirExists=A pasta:%n%n%1%n%njá existe. Pretende instalar nesta pasta?
DirDoesntExistTitle=A pasta não existe
DirDoesntExist=A pasta:%n%n%1%n%nnão existe. Pretende que esta pasta seja criada?

; *** "Select Components" wizard page
WizardSelectComponents=Selecione os componentes
SelectComponentsDesc=Que componentes deverão ser instalados?
SelectComponentsLabel2=Selecione os componentes que quer instalar e desselecione os componentes que não quer instalar. Clique em Seguinte quando estiver pronto para continuar.
FullInstallation=Instalação Completa
; if possible don't translate 'Compact' as 'Minimal' (I mean 'Minimal' in your language)
CompactInstallation=Instalação Compacta
CustomInstallation=Instalação Personalizada
NoUninstallWarningTitle=Componentes Encontrados
NoUninstallWarning=O Assistente de Instalação detetou que os seguintes componentes estão instalados no seu computador:%n%n%1%n%nSe desselecionar estes componentes eles não serão desinstalados.%n%nDeseja continuar?
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=A seleção atual necessita de pelo menos [gb] GB de espaço em disco.
ComponentsDiskSpaceMBLabel=A seleção atual necessita de pelo menos [mb] MB de espaço em disco.

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=Selecione tarefas adicionais
SelectTasksDesc=Que tarefas adicionais deverão ser executadas?
SelectTasksLabel2=Selecione as tarefas adicionais que deseja que o Assistente de Instalação execute na instalação do [name] e em seguida clique em Seguinte.

; *** "Select Start Menu Folder" wizard page
WizardSelectProgramGroup=Selecione a pasta do Menu Iniciar
SelectStartMenuFolderDesc=Onde deverão ser colocados os atalhos do programa?
SelectStartMenuFolderLabel3=Os atalhos do programa serão criados na seguinte pasta do Menu Iniciar.
SelectStartMenuFolderBrowseLabel=Para continuar, clique em Seguinte. Se desejar selecionar uma pasta diferente, clique em Procurar.
MustEnterGroupName=É necessário introduzir um nome para a pasta.
GroupNameTooLong=O nome ou caminho para a pasta é demasiado longo.
InvalidGroupName=O nome da pasta não é válido.
BadGroupName=O nome da pasta não pode conter nenhum dos seguintes caracteres:%n%n%1
NoProgramGroupCheck2=&Não criar nenhuma pasta no Menu Iniciar

; *** "Ready to Install" wizard page
WizardReady=Pronto para Instalar
ReadyLabel1=O Assistente de Instalação está pronto para instalar o [name] no seu computador.
ReadyLabel2a=Clique em Instalar para continuar a instalação, ou clique em Anterior se desejar rever ou alterar alguma das configurações.
ReadyLabel2b=Clique em Instalar para continuar a instalação.
ReadyMemoUserInfo=Informações do utilizador:
ReadyMemoDir=Localização de destino:
ReadyMemoType=Tipo de instalação:
ReadyMemoComponents=Componentes selecionados:
ReadyMemoGroup=Pasta do Menu Iniciar:
ReadyMemoTasks=Tarefas adicionais:

; *** TDownloadWizardPage wizard page and DownloadTemporaryFile
DownloadingLabel2=A transferir ficheiros...
ButtonStopDownload=&Parar transferência
StopDownload=Tem a certeza que deseja parar a transferência?
ErrorDownloadAborted=Transferência cancelada
ErrorDownloadFailed=Falha na transferência: %1 %2
ErrorDownloadSizeFailed=Falha ao obter tamanho: %1 %2
ErrorProgress=Progresso inválido: %1 de %2
ErrorFileSize=Tamanho de ficheiro inválido: esperado %1, encontrado %2

; *** TExtractionWizardPage wizard page and ExtractArchive
ExtractingLabel=A extrair ficheiros...
ButtonStopExtraction=&Parar extração
StopExtraction=Tem a certeza que deseja parar a extração?
ErrorExtractionAborted=Extração cancelada
ErrorExtractionFailed=Falha na extração: %1

; *** Archive extraction failure details
ArchiveIncorrectPassword=A palavra-passe está incorreta
ArchiveIsCorrupted=O ficheiro está corrompido
ArchiveUnsupportedFormat=O formato do ficheiro não é suportado

; *** "Preparing to Install" wizard page
WizardPreparing=A preparar para instalar
PreparingDesc=A preparar para instalar o [name] no seu computador.
PreviousInstallNotCompleted=A instalação/remoção de um programa anterior não foi completada. Necessitará de reiniciar o computador para completar essa instalação.%n%nDepois de reiniciar o computador, execute novamente este Assistente de Instalação para completar a instalação do [name].
CannotContinue=A instalação não pode continuar. Clique em Cancelar para sair.
ApplicationsFound=As seguintes aplicações estão a utilizar ficheiros que necessitam de ser atualizados pelo Assistente de Instalação. Recomenda-se que permita que o Assistente de Instalação feche estas aplicações.
ApplicationsFound2=As seguintes aplicações estão a utilizar ficheiros que necessitam de ser atualizados pelo Assistente de Instalação. Recomenda-se que permita que o Assistente de Instalação feche estas aplicações. Depois de completar a instalação, o Assistente de Instalação tentará reiniciar as aplicações.
CloseApplications=&Fechar as aplicações automaticamente
DontCloseApplications=&Não fechar as aplicações
ErrorCloseApplications=O Assistente de Instalação não conseguiu fechar todas as aplicações automaticamente. Antes de continuar, recomenda-se que feche todas as aplicações que utilizem ficheiros que necessitem de ser atualizados pelo Assistente de Instalação.
PrepareToInstallNeedsRestart=O Assistente de Instalação necessita de reiniciar o seu computador. Depois de reiniciar o computador, execute novamente o Assistente de Instalação para completar a instalação do [name].%n%nDeseja reiniciar agora?

; *** "Installing" wizard page
WizardInstalling=A instalar
InstallingLabel=Aguarde enquanto o Assistente de Instalação instala o [name] no seu computador.

; *** "Setup Completed" wizard page
FinishedHeadingLabel=Instalação do [name] concluída
FinishedLabelNoIcons=O Assistente de Instalação concluiu a instalação do [name] no seu computador.
FinishedLabel=O Assistente de Instalação concluiu a instalação do [name] no seu computador. A aplicação pode ser iniciada através dos ícones de atalho instalados.
ClickFinish=Clique em Concluir para finalizar o Assistente de Instalação.
FinishedRestartLabel=Para completar a instalação do [name], o Assistente de Instalação deverá reiniciar o seu computador. Deseja reiniciar agora?
FinishedRestartMessage=Para completar a instalação do [name], o Assistente de Instalação deverá reiniciar o seu computador.%n%nDeseja reiniciar agora?
ShowReadmeCheck=Sim, desejo visualizar o ficheiro README
YesRadio=&Sim, desejo reiniciar o computador agora
NoRadio=&Não, desejo reiniciar o computador mais tarde
; used for example as 'Run MyProg.exe'
RunEntryExec=Executar %1
; used for example as 'View Readme.txt'
RunEntryShellExec=Visualizar %1

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=O Assistente de Instalação precisa do disco seguinte
SelectDiskLabel2=Introduza o disco %1 e clique em OK.%n%nSe os ficheiros deste disco estiverem num local diferente do mostrado abaixo, indique o caminho correto ou clique em Procurar.
PathLabel=&Caminho:
FileNotInDir2=O ficheiro "%1" não foi encontrado em "%2". Introduza o disco correto ou selecione outra pasta.
SelectDirectoryLabel=Indique a localização do disco seguinte.

; *** Installation phase messages
SetupAborted=A instalação não está completa.%n%nCorrija o problema e execute o Assistente de Instalação novamente.
AbortRetryIgnoreSelectAction=Selecione uma ação
AbortRetryIgnoreRetry=&Tentar novamente
AbortRetryIgnoreIgnore=&Ignorar o erro e continuar
AbortRetryIgnoreCancel=Cancelar a instalação
RetryCancelSelectAction=Selecione uma ação
RetryCancelRetry=&Tentar novamente
RetryCancelCancel=Cancelar

; *** Installation status messages
StatusClosingApplications=A fechar aplicações...
StatusCreateDirs=A criar pastas...
StatusExtractFiles=A extrair ficheiros...
StatusDownloadFiles=A transferir ficheiros...
StatusCreateIcons=A criar atalhos...
StatusCreateIniEntries=A criar entradas em INI...
StatusCreateRegistryEntries=A criar entradas no registo...
StatusRegisterFiles=A registar ficheiros...
StatusSavingUninstall=A guardar informações para desinstalação...
StatusRunProgram=A concluir a instalação...
StatusRestartingApplications=A reiniciar aplicações...
StatusRollback=A anular as alterações...

; *** Misc. errors
ErrorInternal2=Erro interno: %1
ErrorFunctionFailedNoCode=%1 falhou
ErrorFunctionFailed=%1 falhou; código %2
ErrorFunctionFailedWithMessage=%1 falhou; código %2.%n%3
ErrorExecutingProgram=Não é possível executar o ficheiro:%n%1

; *** Registry errors
ErrorRegOpenKey=Erro ao abrir a chave de registo:%n%1\%2
ErrorRegCreateKey=Erro ao criar a chave de registo:%n%1\%2
ErrorRegWriteKey=Erro ao escrever na chave de registo:%n%1\%2

; *** INI errors
ErrorIniEntry=Erro ao criar entradas em INI no ficheiro "%1".

; *** File copying errors
FileAbortRetryIgnoreSkipNotRecommended=&Ignorar este ficheiro (não recomendado)
FileAbortRetryIgnoreIgnoreNotRecommended=&Ignorar este erro e continuar (não recomendado)
SourceIsCorrupted=O ficheiro de origem está corrompido
SourceDoesntExist=O ficheiro de origem "%1" não existe
SourceVerificationFailed=Verificação do ficheiro de origem falhou: %1
VerificationSignatureDoesntExist=O ficheiro de assinatura "%1" não existe
VerificationSignatureInvalid=O ficheiro de assinatura "%1" é inválido
VerificationKeyNotFound=O ficheiro de assinatura "%1" usa uma chave desconhecida
VerificationFileNameIncorrect=O nome do ficheiro é incorreto
VerificationFileTagIncorrect=A tag do ficheiro é incorreta
VerificationFileSizeIncorrect=O tamanho do ficheiro é incorreto
VerificationFileHashIncorrect=O hash do ficheiro é incorreto
ExistingFileReadOnly2=O ficheiro existente não pode ser substituído porque tem o atributo "só de leitura".
ExistingFileReadOnlyRetry=&Remover o atributo "só de leitura" e tentar novamente
ExistingFileReadOnlyKeepExisting=&Manter o ficheiro existente
ErrorReadingExistingDest=Ocorreu um erro ao tentar ler o ficheiro existente:
FileExistsSelectAction=Selecione uma ação
FileExists2=O ficheiro já existe.
FileExistsOverwriteExisting=&Substituir o ficheiro existente
FileExistsKeepExisting=&Manter o ficheiro existente
FileExistsOverwriteOrKeepAll=&Fazer isto para os próximos conflitos
ExistingFileNewerSelectAction=Selecione uma ação
ExistingFileNewer2=O ficheiro existente é mais recente que o que está a ser instalado.
ExistingFileNewerOverwriteExisting=&Substituir o ficheiro existente
ExistingFileNewerKeepExisting=&Manter o ficheiro existente (recomendado)
ExistingFileNewerOverwriteOrKeepAll=&Fazer isto para os próximos conflitos
ErrorChangingAttr=Ocorreu um erro ao tentar alterar os atributos do ficheiro existente:
ErrorCreatingTemp=Ocorreu um erro ao tentar criar um ficheiro na pasta de destino:
ErrorReadingSource=Ocorreu um erro ao tentar ler o ficheiro de origem:
ErrorCopying=Ocorreu um erro ao tentar copiar um ficheiro:
ErrorDownloading=Ocorreu um erro ao tentar transferir um ficheiro:
ErrorExtracting=Ocorreu um erro ao tentar extrair um ficheiro:
ErrorReplacingExistingFile=Ocorreu um erro ao tentar substituir o ficheiro existente:
ErrorRestartReplace=RestartReplace falhou:
ErrorRenamingTemp=Ocorreu um erro ao tentar mudar o nome de um ficheiro na pasta de destino:
ErrorRegisterServer=Não é possível registar o DLL/OCX: %1
ErrorRegSvr32Failed=O RegSvr32 falhou com o código de saída %1
ErrorRegisterTypeLib=Não foi possível registar a biblioteca de tipos: %1

; *** Uninstall display name markings
; used for example as 'My Program (32-bit)'
UninstallDisplayNameMark=%1 (%2)
; used for example as 'My Program (32-bit, All users)'
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32-bit
UninstallDisplayNameMark64Bit=64-bit
UninstallDisplayNameMarkAllUsers=Todos os utilizadores
UninstallDisplayNameMarkCurrentUser=Utilizador atual

; *** Post-installation errors
ErrorOpeningReadme=Ocorreu um erro ao tentar abrir o ficheiro README.
ErrorRestartingComputer=O Assistente de Instalação não consegue reiniciar o computador. Por favor reinicie manualmente.

; *** Uninstaller messages
UninstallNotFound=O ficheiro "%1" não existe. Não é possível desinstalar.
UninstallOpenError=Não foi possível abrir o ficheiro "%1". Não é possível desinstalar.
UninstallUnsupportedVer=O ficheiro de registo de desinstalação "%1" está num formato que não é reconhecido por esta versão do desinstalador. Não é possível desinstalar
UninstallUnknownEntry=Foi encontrada uma entrada desconhecida (%1) no ficheiro de registo de desinstalação
ConfirmUninstall=Tem a certeza que deseja remover completamente o %1 e todos os seus componentes?
UninstallOnlyOnWin64=Esta desinstalação só pode ser realizada na versão de 64 bits do Windows.
OnlyAdminCanUninstall=Esta desinstalação só pode ser realizada por um utilizador com privilégios administrativos.
UninstallStatusLabel=Por favor aguarde enquanto o %1 está a ser removido do seu computador.
UninstalledAll=O %1 foi removido do seu computador com sucesso.
UninstalledMost=A desinstalação do %1 está concluída.%n%nAlguns elementos não puderam ser removidos. Estes elementos podem ser removidos manualmente.
UninstalledAndNeedsRestart=Para completar a desinstalação do %1, o computador deve ser reiniciado.%n%nDeseja reiniciar agora?
UninstallDataCorrupted=O ficheiro "%1" está corrompido. Não é possível desinstalar

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=Remover ficheiro partilhado?
ConfirmDeleteSharedFile2=O sistema indica que o seguinte ficheiro partilhado já não está a ser utilizado por nenhum programa. Deseja removê-lo?%n%nSe algum programa ainda necessitar deste ficheiro, poderá não funcionar corretamente depois de o remover. Se não tiver a certeza, selecione Não. Manter o ficheiro não causará nenhum problema.
SharedFileNameLabel=Nome do ficheiro:
SharedFileLocationLabel=Localização:
WizardUninstalling=Estado da desinstalação
StatusUninstalling=A desinstalar o %1...

; *** Shutdown block reasons
ShutdownBlockReasonInstallingApp=A instalar %1.
ShutdownBlockReasonUninstallingApp=A desinstalar %1.

; The custom messages below aren't used by Setup itself, but if you make
; use of them in your scripts, you'll want to translate them.

[CustomMessages]

NameAndVersion=%1 versão %2
AdditionalIcons=Atalhos adicionais:
CreateDesktopIcon=Criar atalho no Ambiente de &Trabalho
CreateQuickLaunchIcon=&Criar atalho na barra de Iniciação Rápida
ProgramOnTheWeb=%1 na Web
UninstallProgram=Desinstalar o %1
LaunchProgram=Executar o %1
AssocFileExtension=Associa&r o %1 aos ficheiros com a extensão %2
AssocingFileExtension=A associar o %1 aos ficheiros com a extensão %2...
AutoStartProgramGroupDescription=Inicialização Automática:
AutoStartProgram=Iniciar %1 automaticamente
AddonHostProgramNotFound=Não foi possível localizar %1 na pasta selecionada.%n%nDeseja continuar de qualquer forma?
