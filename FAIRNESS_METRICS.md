# Métricas de Justiça (Fairness) para Algoritmos de Escalonamento

Este documento explica como medir e avaliar a **justiça** dos algoritmos de escalonamento no projeto.

## Definições de Justo

### 1. **Fairness no Tempo de Espera (Waiting Time)**
**Conceito**: Todos os comandos devem esperar +/- o mesmo tempo antes de começar a executar.

**Por quê?**: Um comando que espera muito mais que outros é prejudicado, mesmo que execute rápido.

**Métricas**:
- **Coefficient of Variation (CV)**: `std_dev / mean`
  - Quanto menor, mais justo
  - 0 = perfeitamente justo (todos esperam igual)
  - > 1 = muito injusto (grandes variações)

- **Jain's Fairness Index**: `(Σx)² / (n × Σx²)`
  - Varia de 0 a 1
  - 1.0 = perfeitamente justo
  - 1/n = completamente injusto
  - Mais sensível a outliers extremos

- **Max/Min Ratio**: `max_waiting / min_waiting`
  - Se um comando espera 2s e outro 10s, ratio = 5
  - Quanto mais próximo de 1, mais justo

### 2. **Fairness no Tempo de Resposta (Turnaround Time)**
**Conceito**: Todos os comandos devem ter tempo total semelhante (espera + execução).

**Por quê?**: Alguns usuários podem receber comandos que executam rápido mas esperam muito, enquanto outros recebem comandos que executam devagar mas esperam pouco.

**Métricas**: Mesmas que acima, mas aplicadas a `turnaround_ms`

### 3. **Fairness Per-Usuário**
**Conceito**: Cada usuário deve ter acesso justo ao sistema.

**Por quê?**: Um algoritmo pode ser justo em média, mas prejudicar um usuário específico.

**Análise**:
- Compare o tempo médio de turnaround para cada user_id
- Desvio padrão entre usuários
- Qual usuário é mais prejudicado?

### 4. **Slowdown (Normalized Turnaround Time)** ⭐ CRÍTICO
**Conceito**: Quanto um comando foi prejudicado pelo escalonamento, normalizado pelo tempo de execução.

**Fórmula**: `Slowdown = Turnaround_Time / Service_Time (Duration)`

**Interpretação**:
- `Slowdown = 1.0`: Comando executado imediatamente, sem espera (ideal)
- `Slowdown = 2.0`: Comando demorou 2x mais do que deveria (espera = duration)
- `Slowdown = 10.0`: Comando demorou 10x mais (muito prejudicado)

**Por quê é tão importante?**: 
- Um comando rápido (10ms) que espera 100ms tem slowdown = 11
- Um comando lento (1000ms) que espera 100ms tem slowdown = 1.1
- Slowdown normaliza essa diferença, mostrando impacto real no usuário

**Métricas**:
- **Mean Slowdown**: Média de quantas vezes mais lento ficou cada comando
  - < 2.0: Excelente (máx 1x de espera)
  - 2.0-5.0: Bom
  - > 5.0: Ruim
  
- **P95 Slowdown**: Como os piores 5% dos comandos foram afetados
  - Detecta se algoritmo está falhando para alguns casos
  
- **P99 Slowdown**: Como o pior 1% foi afetado (extremos)
  - Crítico para SLAs em produção
  
- **CV_Slowdown**: Fairness no slowdown (todos sofrem igual impacto?)
  - Baixo = todos sofrem aproximadamente igual
  - Alto = alguns sofrem muito mais que outros

### 5. **Percentis (P95, P99)** ⭐ PROFISSIONAL
**Conceito**: Comportamento dos piores casos, não apenas a média.

**Por quê?**: 
- Média de 10ms com P99 de 500ms é desonesto
- Uma métrica boa para SLA (Service Level Agreement) considera P99

**Interpretação**:
- **P95**: Pior 5% dos comandos - Afeta 5 em 100
- **P99**: Pior 1% dos comandos - Afeta 1 em 100
- **Mean**: Valor "típico" (enganoso se houver outliers)

**Exemplo Real**:
```
FCFS: mean_wait = 50ms, p99_wait = 5000ms ❌
  → 99% espera pouco, mas 1% espera muito (injusto!)

RR: mean_wait = 500ms, p99_wait = 600ms ✅
  → Todos esperam similar, mais justo
```

## Interpretação dos Resultados

### Cenário 1: CV_WAIT Baixo, JAIN_WAIT Alto
✅ **Bom**: Todos os comandos esperam tempo similar → **JUSTO NO TEMPO DE ESPERA**

### Cenário 2: CV_TURNAROUND Alto, JAIN_TURNAROUND Baixo
❌ **Ruim**: Alguns comandos têm tempo total muito maior → **INJUSTO NO TEMPO TOTAL**

### Cenário 3: MAX_MIN_RATIO > 5
⚠️ **Cuidado**: Alguns comandos esperam 5x mais que outros

### Cenário 4: Um usuário com turnaround 10x maior
❌ **Crítico**: Algoritmo está discriminando um usuário específico

### Cenário 5: MEAN_SLOWDOWN = 2.0, P99_SLOWDOWN = 50.0 ⚠️ ALERTA PROFISSIONAL
❌ **Muito Ruim**: 
- Em média comandos sofrem 2x delay (aceitável)
- MAS o pior 1% sofre 50x delay (INACEITÁVEL)
- Exemplo: 99 usuários não sentem diferença, 1 usuário espera infinitamente
- **Conclusão**: Algoritmo FALHA para casos extremos

### Cenário 6: MEAN_SLOWDOWN = 3.0, P99_SLOWDOWN = 3.5, CV_SLOWDOWN = 0.1 ✅ EXCELENTE
✅ **Muito Bom**:
- Todos sofrem ~3x delay (consistente)
- Pior 1% sofre apenas um pouco mais
- CV baixo = muito justo (todos sofrem igual)
- **Conclusão**: Algoritmo EQUILIBRADO E PREVISÍVEL

## Trade-offs Comuns

| Algoritmo | Throughput | Mean Wait | P99 Wait | Mean Slowdown | P99 Slowdown | CV Slowdown | Strengths | Weaknesses |
|-----------|-----------|-----------|----------|---------------|--------------|-------------|-----------|-----------|
| **FCFS** | Alto ⭐⭐⭐ | Baixa | Muito Alta ❌ | ~2.0 | > 10 ❌ | Alta | Simples, previsível | Injusto com variação de duração |
| **Round Robin** | Médio | Alta | Média ✅ | ~5.0 | ~6.0 ✅ | Baixa ✅ | Muito justo | Pior throughput |
| **LCF** | Alto ⭐⭐ | Média | Média | ~3.0 | ~5.0 | Média | Favorece comandos rápidos | Prejudica lentos |

**Leitura Crítica**:
- Procure por **P99 Slowdown baixo** (< 5.0) = algoritmo não desabusava nenhum comando
- **CV Slowdown baixo** = todos sofrem aproximadamente igual impacto

## Recomendações por Caso de Uso

### 🎯 Prioridade: Máxima Justiça
- Usar **Round Robin**
- Verificar CV_WAIT < 0.5 e JAIN_WAIT > 0.9
- Aceitar throughput menor

### 🏆 Prioridade: Justiça + Performance
- Usar **Round Robin com quantum adaptativo**
- Verificar JAIN_TURNAROUND > 0.8
- Monitorar per-user fairness

### ⚡ Prioridade: Throughput Máximo
- Usar **FCFS**
- Aceitar injustiça no tempo de espera
- Mas garantir que nenhum usuário é completamente negligenciado

### 💼 Prioridade: Equilibrado (Mais Comum)
- Usar **LCF com limite de espera máxima**
- CV_WAIT < 1.0
- JAIN_TURNAROUND > 0.7
- Nenhum usuário com espera > 2x a média

## Como Usar os Novos Gráficos

### Métricas Básicas (Tempo)
1. **fairness_cv_waiting.png**: Compara injustiça no tempo de espera
   - Procure linhas baixas (mais justo)
   
2. **fairness_jain_waiting.png**: Visão alternativa de fairness na espera
   - Procure linhas próximas a 1.0 (mais justo)
   
3. **fairness_cv_turnaround.png** / **fairness_jain_turnaround.png**: Avalia tempo total
   - Mais importante para experiência do usuário

### Métricas Profissionais (Slowdown) ⭐
4. **slowdown_mean.png**: Média de slowdown por policy
   - Quanto os comandos foram prejudicados em média?
   - Procure por políticas com valores < 2.0 (excelente)
   
5. **slowdown_p95.png**: Pior 5% dos comandos
   - Se P95 está alto mas Mean está baixo = alguns comandos sofrem muito
   - Procure por valores < 3.0

6. **slowdown_p99.png**: Pior 1% (crítico para SLA!)
   - Mostra impacto nos piores casos
   - Procure por valores < 5.0
   - Se alguma linha está em 100+ = algoritmo falhou gravemente para alguns

7. **slowdown_cv.png**: Fairness no slowdown
   - Todos sofrem aproximadamente igual impacto?
   - Baixo = justo, Alto = injusto

8. **slowdown_distribution.png**: Box plot de todos os slowdowns
   - Linha vermelha em 1.0 = ideal (sem delay)
   - Mediana perto de 1.0 = a maioria não sofreu
   - Muitos outliers acima = alguns comandos foram muito prejudicados

### User-Level Fairness
9. **user_fairness_*.png**: Mostra se um usuário é prejudicado
   - Procure barras com altura similar
   - Linhas vermelhas mostram a média
   - Se um usuário está 2x acima = está sendo discriminado

## Próximos Passos

1. **Execute a análise**:
   ```bash
   python3 analysis/analyze_stats.py tmp/stats.csv analysis/output
   ```

2. **Compare os CSVs**:
   - Abra `analysis/output/summary_metrics.csv`
   - Procure por algoritmo com melhor JAIN_TURNAROUND

3. **Visualize os gráficos**:
   - Verifique se as linhas de cada algoritmo são planas (estável) ou oscilantes (instável)
   - Compare CV/Jain entre políticas

4. **Defina sua conclusão**:
   - Qual algoritmo é mais justo?
   - Qual tem melhor throughput?
   - Qual recomenda para produção?

## Guia de Leitura do CSV (summary_metrics.csv)

### Colunas Críticas em Ordem de Importância

**NÍVEL 1: Decisão Principal (Leia Sempre)**
1. **mean_slowdown**: Impacto médio no usuário
   - < 1.5: Excelente, < 2.0: Bom, < 5.0: Aceitável, > 5.0: Ruim

2. **p99_slowdown**: Impacto no pior 1%
   - < 3.0: Excelente, < 5.0: Bom, < 10.0: Aceitável, > 10.0: Ruim
   - **CRÍTICO**: Se p99 > 20, algoritmo falha para alguns casos

3. **cv_slowdown**: Fairness (todos sofrem igual?)
   - < 0.3: Muito Justo, < 0.5: Justo, < 1.0: Aceitável, > 1.0: Injusto

**NÍVEL 2: Complementar (Confirme Tendências)**
4. **jain_turnaround**: Fairness geral no tempo total
   - Quanto maior, melhor (max 1.0)
   - Compare qual algoritmo está acima de 0.8

5. **mean_turnaround_ms**: Tempo total médio
   - Afeta indiretamente o throughput
   - Mais baixo = sistema mais rápido

6. **p99_wait_ms**: Pior tempo de espera individual
   - Detecta injustiça extrema na espera

**NÍVEL 3: Técnico (Para Análise Profunda)**
7. Outras métricas de CV/Jain para especificar o tipo de injustiça

### Exemplo de Análise

```
FCFS:  mean_slowdown=2.1, p99_slowdown=150, cv_slowdown=2.3
       → Injusto! Alguns comandos sofrem muito. REJEITADO.

RR:    mean_slowdown=4.0, p99_slowdown=4.5, cv_slowdown=0.1
       → Muito justo! Todos sofrem similar. BOM CANDIDATO.

LCF:   mean_slowdown=2.5, p99_slowdown=8.0, cv_slowdown=0.6
       → Equilibrado. Bom para throughput mas algumas injustiças.
```

### Checklist Profissional

- [ ] p99_slowdown < 5.0 (não abandona casos extremos)
- [ ] cv_slowdown < 0.5 (fairness no impacto)
- [ ] mean_slowdown < 3.0 (impacto aceitável)
- [ ] Nenhum usuário com turnaround > 2x a média
- [ ] Throughput adequado para o caso de uso

## Fórmulas Matemáticas

### Coefficient of Variation (CV)
```
CV = σ / μ
onde:
  σ = desvio padrão dos tempos
  μ = média dos tempos
```

### Jain's Fairness Index (JFI)
```
JFI = (Σx_i)² / (n * Σx_i²)
onde:
  x_i = tempo de cada comando
  n = número de comandos
```

### Max/Min Ratio
```
Ratio = max(x_i) / min(x_i)
```

### Slowdown (Normalized Turnaround Time)
```
Slowdown = Turnaround_Time / Service_Time
onde:
  Turnaround_Time = end_time - submit_time
  Service_Time = duration (tempo de execução)

Interpretação:
  1.0 = sem delay (ideal)
  2.0 = demorou 2x mais (tempo de espera = tempo de execução)
  N.0 = demorou N vezes mais
```

### Percentil
```
P_k = valor tal que k% dos dados estão abaixo dele

P95 = valor que separa os piores 5%
P99 = valor que separa o pior 1%

Para array ordenado:
  P_k = valores_ordenados[floor(k/100 * n)]
```

## Referências

- Jain, R. K., Chiu, D. M., & Hawe, W. R. (1984). "A Quantitative Measure of Fairness and Discrimination for Resource Allocation in Shared Computer Systems"
