#include <iostream>
#include <vector>
#include <iomanip>
#include <random>
#include <chrono>

using namespace std;
using namespace std::chrono;

/**
 * CONFIGURACIÓN GLOBAL DEL SISTEMA DE CACHÉ
 */
const int TAMANO_CACHE = 8192; // Tamaño total de la caché: 8 KB
const int TAMANO_BLOQUE = 32;  // Tamaño de cada línea/bloque: 32 Bytes
const int COSTO_HIT = 1;       // Costo en ciclos de reloj para un acierto (hit)
const int COSTO_MISS = 100;    // Costo en ciclos de reloj para un fallo (miss) - Penalización de memoria principal

/**
 * Enumeración para definir los tipos de patrones de acceso a memoria (Trazas)
 */
enum TipoTraza {
    ALEATORIA,          // Sin un patrón definido
    LOCALIDAD_TEMPORAL, // Reacceso a direcciones usadas recientemente
    LOCALIDAD_ESPACIAL, // Acceso a direcciones contiguas o cercanas
    MIXTA_FASES         // Combina diferentes comportamientos por etapas
};

/**
 * Enumeración para las políticas de reemplazo de bloques en la caché
 */
enum PoliticaReemplazo { 
    LRU,  // Least Recently Used: Reemplaza el bloque usado hace más tiempo
    FIFO, // First In, First Out: Reemplaza el bloque que llegó primero
    LFU   // Least Frequently Used: Reemplaza el bloque con menos accesos
};

/**
 * Estructura que representa un bloque individual (línea) dentro de la caché
 */
struct BloqueCache {
    bool valido = false;       // Bit de validez: Indica si el bloque contiene datos útiles
    unsigned int etiqueta = 0; // Etiqueta (tag) para identificar la dirección de memoria
    
    long long ultimo_uso = 0;    // Marca de tiempo para la política LRU
    long long tiempo_llegada = 0; // Marca de tiempo para la política FIFO
    int frecuencia_uso = 0;      // Contador para la política LFU
};

/**
 * Estructura que representa un conjunto (set) de la caché asociativa
 */
struct ConjuntoCache {
    vector<BloqueCache> bloques; // Lista de bloques que componen el conjunto (vías)

    //Inicializa el conjunto con el número de vías especificado
    ConjuntoCache(int vias) : bloques(vias) {} 
};

/**
 * Clase principal que simula el comportamiento de una caché asociativa por conjuntos
 */
class SimuladorCache {
private:
    vector<ConjuntoCache> cache; // Estructura de datos de la caché
    int vias;                    // Grado de asociatividad
    int num_conjuntos;           // Número total de conjuntos en la caché
    long long reloj_global;      // Contador global para simular el paso del tiempo
    long long hits;              // Contador de aciertos
    long long misses;            // Contador de fallos

public:
    /**
     * Constructor del simulador
     * Calcula el número de conjuntos basado en el tamaño total, el bloque y las vías.
     */
    SimuladorCache(int v) : vias(v), reloj_global(0), hits(0), misses(0) {
        num_conjuntos = TAMANO_CACHE / (TAMANO_BLOQUE * vias);
        cache.assign(num_conjuntos, ConjuntoCache(vias));
    }

    /**
     * Reinicia las estadísticas y el estado de la caché para una nueva prueba
     */
    void reiniciar() {
        cache.assign(num_conjuntos, ConjuntoCache(vias));
        reloj_global = 0;
        hits = 0;
        misses = 0;
    }

    /**
     * Simula un acceso a una dirección de memoria específica
     */
    void accederMemoria(unsigned int direccion, PoliticaReemplazo politica) {
        reloj_global++;

        // Cálculo del índice (en qué conjunto mapea) y la etiqueta (qué bloque es)
        unsigned int indice = (direccion / TAMANO_BLOQUE) % num_conjuntos;
        unsigned int etiqueta = direccion / (TAMANO_BLOQUE * num_conjuntos);

        bool hit = false;
        int indice_bloque_reemplazar = -1;
        int bloque_vacio = -1;

        // Buscar si la dirección ya está en la caché (Hit)
        for (int i = 0; i < vias && !hit; i++) {
            if (cache[indice].bloques[i].valido) {
                if (cache[indice].bloques[i].etiqueta == etiqueta) {
                    hit = true;
                    hits++;
                    // Actualizar metadatos para las políticas de reemplazo
                    cache[indice].bloques[i].ultimo_uso = reloj_global; 
                    cache[indice].bloques[i].frecuencia_uso++;      
                }
            } else if (bloque_vacio == -1) {
                // Registrar el primer bloque libre encontrado por si hay un fallo
                bloque_vacio = i;
            }
        }

        // Si no se encontró el dato (Miss)
        if (!hit) {
            misses++;
            if (bloque_vacio != -1) {
                // Si hay espacio, simplemente ocupamos el bloque vacío
                indice_bloque_reemplazar = bloque_vacio;
            } else {
                // Si el conjunto está lleno, invocar la política de reemplazo para elegir una "víctima"
                indice_bloque_reemplazar = encontrarVictima(indice, politica);
            }

            // Cargar el nuevo bloque en la caché
            cache[indice].bloques[indice_bloque_reemplazar].valido = true;
            cache[indice].bloques[indice_bloque_reemplazar].etiqueta = etiqueta;
            cache[indice].bloques[indice_bloque_reemplazar].ultimo_uso = reloj_global;
            cache[indice].bloques[indice_bloque_reemplazar].tiempo_llegada = reloj_global;
            cache[indice].bloques[indice_bloque_reemplazar].frecuencia_uso = 1;
        }
    }

    /**
     * Determina qué bloque debe ser expulsado según la política seleccionada
     */
    int encontrarVictima(int indice_conjunto, PoliticaReemplazo politica) {
        int victima = 0;
        for (int i = 1; i < vias; i++) {
            if (politica == LRU) {
                // LRU: El que tiene el menor tiempo de último uso
                if (cache[indice_conjunto].bloques[i].ultimo_uso < cache[indice_conjunto].bloques[victima].ultimo_uso)
                    victima = i;
            } 
            else if (politica == FIFO) {
                // FIFO: El que tiene el menor tiempo de llegada (el más viejo)
                if (cache[indice_conjunto].bloques[i].tiempo_llegada < cache[indice_conjunto].bloques[victima].tiempo_llegada)
                    victima = i;
            } 
            else if (politica == LFU) {
                // LFU: El que tiene la menor frecuencia de acceso
                if (cache[indice_conjunto].bloques[i].frecuencia_uso < cache[indice_conjunto].bloques[victima].frecuencia_uso) {
                    victima = i;
                } else if (cache[indice_conjunto].bloques[i].frecuencia_uso == cache[indice_conjunto].bloques[victima].frecuencia_uso) {
                    // Desempate para LFU: Usar LRU si las frecuencias son iguales
                    if (cache[indice_conjunto].bloques[i].ultimo_uso < cache[indice_conjunto].bloques[victima].ultimo_uso)
                        victima = i;
                }
            }
        }
        return victima;
    }

    /**
     * Muestra por pantalla los resultados de la simulación
     */
    void imprimirEstadisticas(string nombre_politica, double tiempo_real_ms) {
        long long accesos_totales = hits + misses;
        double hit_ratio = (double)hits / accesos_totales * 100.0;
        // Cálculo teórico de ciclos basado en costos definidos
        long long ciclos_totales = (hits * COSTO_HIT) + (misses * COSTO_MISS);

        cout << "   -> Politica: " << left << setw(4) << nombre_politica 
             << " | Hit Ratio: " << fixed << setprecision(2) << hit_ratio << "%"
             << " | Ciclos CPU (Teoricos): " << ciclos_totales 
             << " | Tiempo Real Ejecucion: " << tiempo_real_ms << " ms\n";
    }
};

/**
 * Genera una secuencia de direcciones de memoria basadas en modelos sintéticos de comportamiento
 */
vector<unsigned int> generarTrazas(int cantidad, TipoTraza tipo) {
    vector<unsigned int> trazas;
    trazas.reserve(cantidad);

    random_device rd;
    mt19937 gen(rd());

    // Distribuciones estadísticas para generar diferentes comportamientos
    uniform_int_distribution<unsigned int> distGlobal(0, 1000000); // Rango amplio
    uniform_int_distribution<unsigned int> distHot(0, 4095);       // Zona de memoria pequeña (Working Set)
    uniform_int_distribution<unsigned int> distPaso(0, 7);         // Saltos pequeños para localidad espacial
    uniform_int_distribution<unsigned int> distProb(1, 100);       // Para decisiones probabilísticas

    unsigned int base = 0;

    for (int i = 0; i < cantidad; i++) {
        unsigned int direccion = 0;

        switch (tipo) {
            case ALEATORIA:
                // Accesos sin ningún orden, muy mala para la caché
                direccion = distGlobal(gen);
                break;

            case LOCALIDAD_TEMPORAL:
                // Regla 80/20: el 80% de los accesos ocurren en una zona pequeña de memoria
                if (distProb(gen) <= 80) {
                    direccion = distHot(gen) * TAMANO_BLOQUE;
                } else {
                    direccion = distGlobal(gen);
                }
                break;

            case LOCALIDAD_ESPACIAL:
                // Accesos secuenciales o muy cercanos entre sí (como recorrer un array)
                if (i % 32 == 0) {
                    base = distGlobal(gen);
                }
                direccion = base + distPaso(gen) * 4;
                break;

            case MIXTA_FASES:
                // Simula un programa que cambia de contexto o fase de ejecución
                if (i < cantidad / 2) {
                    direccion = (distHot(gen) % 2048) * TAMANO_BLOQUE;
                } else {
                    direccion = ((distHot(gen) % 2048) + 50000) * TAMANO_BLOQUE;
                }
                break;
        }

        trazas.push_back(direccion);
    }

    return trazas;
}

/**
 * Ejecuta la simulación para una configuración de vías dada y compara las políticas
 */
void evaluarConfiguracion(int vias, const vector<unsigned int>& trazas) {
    cout << "\n=== Evaluando Caché con Asociatividad de " << vias << " vias ===\n";
    SimuladorCache simulador(vias);

    // Lista de políticas a probar
    pair<PoliticaReemplazo, string> politicas[] = {
        {LRU, "LRU"}, {FIFO, "FIFO"}, {LFU, "LFU"}
    };

    for (auto& p : politicas) {
        auto inicio = high_resolution_clock::now();
        
        // Ejecución de la traza sobre el simulador
        for (unsigned int dir : trazas) {
            simulador.accederMemoria(dir, p.first);
        }
        
        auto fin = high_resolution_clock::now();
        duration<double, std::milli> tiempo_real = fin - inicio;

        simulador.imprimirEstadisticas(p.second, tiempo_real.count());
        simulador.reiniciar(); // Limpiar caché para la siguiente política
    }
}

/**
 * Función principal: Configura los experimentos y lanza las simulaciones
 */
int main() {
    // Escenarios de prueba: diferentes volúmenes de datos
    vector<int> cantidades = {100, 500, 10000, 50000, 500000, 1000000};
    // Diferentes configuraciones de vías para evaluar el impacto de la asociatividad
    vector<int> pruebas_vias = {2, 4, 8, 16};

    // Diferentes tipos de carga de trabajo (Workloads)
    vector<pair<TipoTraza, string>> tipos = {
        {ALEATORIA, "Aleatoria"},
        {LOCALIDAD_TEMPORAL, "Localidad temporal"},
        {LOCALIDAD_ESPACIAL, "Localidad espacial"},
        {MIXTA_FASES, "Mixta por fases"}
    };

    // Bucle anidado para probar todas las combinaciones posibles
    for (auto& tipo : tipos) {
        cout << "\n==============================\n";
        cout << "Tipo de traza: " << tipo.second << "\n";
        cout << "==============================\n";

        for (int cantidad : cantidades) {
            cout << "\nCantidad de trazas: " << cantidad << "\n";
            // Generar la carga de trabajo una sola vez por cantidad/tipo
            vector<unsigned int> trazas = generarTrazas(cantidad, tipo.first);

            for (int vias : pruebas_vias) {
                evaluarConfiguracion(vias, trazas);
            }
        }
    }

    return 0;
}
