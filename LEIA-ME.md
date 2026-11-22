# Nosfera — como compilar e usar no FL Studio (Windows)

Este pacote tem o código completo do plugin **Nosfera** (VST3 + versão independente).
A interface é a mesma do protótipo, e o som é gerado em C++ dentro do plugin.

Faça **um passo de cada vez**. Cada comando vai entre as caixas cinza: copie, cole no
PowerShell e aperte Enter.

---

## 1. Instalar as ferramentas (só na primeira vez)

1. **Visual Studio 2022 Community** (grátis): https://visualstudio.microsoft.com/pt-br/vs/community/
   Na instalação, marque **"Desenvolvimento para desktop com C++"**. É grande (uns 8 GB).
2. **CMake**: https://cmake.org/download/ — baixe o instalador do Windows (.msi).
   Na instalação, marque **"Add CMake to the system PATH"**.
3. **Git**: https://git-scm.com/download/win — pode deixar tudo no padrão.

Depois de instalar os três, **reinicie o computador**.

## 2. Instalar o WebView2 (é o que mostra a interface dentro do plugin)

Abra o **PowerShell** (procure "PowerShell" no menu Iniciar) e rode:

```
Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
```
> Registra o site de pacotes da Microsoft. Se perguntar algo, responda **S** (ou **Y**).

```
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository
```
> Baixa a biblioteca do WebView2 que o plugin usa. Responda **S** (ou **Y**) se perguntar.

## 3. Colocar o projeto no lugar

Extraia o arquivo **Nosfera-projeto.zip** em `C:\Nosfera` (a pasta deve conter o arquivo `CMakeLists.txt`).

```
cd C:\Nosfera
```
> Entra na pasta do projeto.

## 4. Compilar

```
cmake -B build -G "Visual Studio 17 2022" -A x64
```
> Prepara o projeto. Na primeira vez ele baixa o JUCE sozinho (precisa de internet, leva alguns minutos).

```
cmake --build build --config Release --target Nosfera_VST3
```
> Compila o plugin. A primeira vez pode levar de 10 a 20 minutos.

Quando terminar, o plugin estará em `C:\Nosfera\build\Nosfera_artefacts\Release\VST3\Nosfera.vst3`.

## 5. Instalar no FL Studio

Feche o PowerShell e abra outro **como administrador** (botão direito no PowerShell → "Executar como administrador"):

```
Copy-Item -Recurse -Force "C:\Nosfera\build\Nosfera_artefacts\Release\VST3\Nosfera.vst3" "C:\Program Files\Common Files\VST3\"
```
> Copia o plugin para a pasta padrão de VST3 do Windows.

No FL Studio:
1. **Options → Manage plugins → Find installed plugins**.
2. Procure **Nosfera** na lista e marque a estrela (favorito).
3. No Channel Rack, clique em **+** e escolha **Nosfera**.

## 6. Usando

- Os **150 presets** aparecem no navegador do plugin e também no menu de presets do FL (setinhas do plugin).
- As notas do **piano roll** e do seu teclado MIDI acendem as teclas na interface.
- Todos os knobs podem ser **automatizados** no FL (botão direito no knob do FL → Create automation clip).
- Ao salvar o projeto do FL, o som e o preset ficam salvos junto.

## Testar sem o FL (opcional)

```
cmake --build build --config Release --target Nosfera_Standalone
```
> Cria um programa que abre o Nosfera sozinho, em `build\Nosfera_artefacts\Release\Standalone\Nosfera.exe`.

---

## O que já foi testado

- Os 150 presets geram som, sem silêncio, sem distorção e sem erros.
- O plugin passou no **pluginval** (validador usado pela indústria) no nível máximo de rigor (10).
- A janela do plugin abre com a interface completa.
- Salvar e restaurar o projeto mantém o preset e todos os knobs.
- Os testes foram feitos numa máquina Linux. A compilação no Windows usa o mesmo código, mas
  se aparecer algum erro, me mande o texto e eu corrijo.

## Licença do JUCE

O JUCE é gratuito para começar, mas tem regras de licença para produtos vendidos.
Antes de vender o Nosfera, confira a licença em https://juce.com/get-juce/.

## Estrutura do projeto

- `Source/SynthEngine.h` — motor de som (osciladores, filtro, envelopes, efeitos).
- `Source/Presets.h` — os 150 presets.
- `Source/Params.h` — os parâmetros (knobs) que o FL enxerga.
- `Source/PluginProcessor.*` — a parte de áudio do plugin.
- `Source/PluginEditor.*` — a janela do plugin (mostra a interface `ui/index.html`).
- `tests/render_test.cpp` — teste que toca todos os presets e mede o som.
