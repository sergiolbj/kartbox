// ==========================================
//   KARTBOX PRO - BOTÕES COM ENCAIXE QUADRADO
// ==========================================

// --- 1. DIMENSÕES TÉCNICAS (LCD 4.3") ---
tela_l = 104; 
tela_c = 62.0;
margem_parafuso = 12.0;      
espaco_botoes = 15.0;       
gap_topo_tela = 2.0;        
deslocamento_geral_x = 2.0; 
distancia_entre_botoes = 30.0; 

// AJUSTE: Offset exclusivo para a peça do meio (suporte da tela)
offset_x_tampa_meio = 0.0; 

// AJUSTE: Offset vertical dos botões
offset_y_botoes = -2.0; 

// --- 2. CONFIGURAÇÃO DO BERÇO E DO PINO (ANTI-ROTAÇÃO) ---
dim_switch = 6.6;           // Encaixe interno do berço
parede_berco = 1.2;         
altura_parede_berco = 8.0;  // Profundidade do guia do botão

// Configuração do Botão (Pino)
dim_base_pino = dim_switch - 0.4; // Folga de 0.2mm de cada lado para deslizar
altura_base_pino = 2.5;           // Altura da parte quadrada que impede a rotação

// --- 3. CONFIGURAÇÃO DA MOLDURA SUPERIOR (A CASCA) ---
espessura_teto_moldura = 1.2;   
espessura_parede_moldura = 2; 
altura_interna_moldura = 1.2;   

// --- 4. AJUSTE DO VISOR (MOLDURA SUPERIOR) ---
moldura_janela_l = 94.0;    
moldura_janela_c = 57.0;    
moldura_h_offset = 0.0;     
moldura_v_offset = 6.5;     

// --- 5. ESTRUTURA DA BASE E TAMPA DO MEIO ---
espessura_tampa_meio = 1.5;      
altura_interna_base = 25.0; 
espessura_parede_caixa = 2.5;     
altura_strap = 3.5;         
parede_blindagem = 1.2;     

// --- 6. HARDWARE E CINTAS ---
btn_furo_diam = 3.5;        
diam_furo_m3 = 3.4;         
diam_insercao_m3 = 4.0;     
prof_insercao_m3 = 6.0;     
pos_strap_horizontal = 15;   
largura_strap = 22.0;       

// ==========================================
//   CÁLCULOS AUTOMÁTICOS
// ==========================================
largura = tela_l + (espessura_parede_caixa * 2) + (margem_parafuso * 2);
comprimento = tela_c + espaco_botoes + (espessura_parede_caixa * 2); 
altura_total_base = altura_interna_base + espessura_parede_caixa;
espessura_total_moldura = altura_interna_moldura + espessura_teto_moldura;
raio_canto = 5;
$fn = 60;

pos_x_parafuso = [raio_canto, largura - raio_canto];
pos_y_parafuso = [raio_canto, comprimento - raio_canto];

cy_strap = (comprimento/2) + pos_strap_horizontal;
cx_strap = largura/2; 

pos_y_botoes_base = espessura_parede_caixa + (espaco_botoes/2) + offset_y_botoes;

// ==========================================
//   MÓDULOS
// ==========================================

module base() {
    difference() {
        union() {
            difference() {
                hull() {
                    for(x=pos_x_parafuso, y=pos_y_parafuso) translate([x, y, 0]) cylinder(h=altura_total_base, r=raio_canto);
                }
                translate([espessura_parede_caixa, espessura_parede_caixa, espessura_parede_caixa])
                hull() {
                    ri = max(0.1, raio_canto - espessura_parede_caixa);
                    for(x=[ri, largura-espessura_parede_caixa*2-ri], y=[ri, comprimento-espessura_parede_caixa*2-ri])
                        translate([x, y, 0]) cylinder(h=altura_interna_base + 1, r=ri);
                }
            }
            intersection() {
                translate([espessura_parede_caixa, espessura_parede_caixa, espessura_parede_caixa])
                    cube([largura-espessura_parede_caixa*2, comprimento-espessura_parede_caixa*2, altura_strap + parede_blindagem]);
                union() {
                    translate([0, cy_strap - (largura_strap/2 + parede_blindagem), espessura_parede_caixa]) 
                        cube([largura, largura_strap + (parede_blindagem * 2), altura_strap + parede_blindagem]);
                    translate([cx_strap - (largura_strap/2 + parede_blindagem), 0, espessura_parede_caixa]) 
                        cube([largura_strap + (parede_blindagem * 2), comprimento, altura_strap + parede_blindagem]);
                }
            }
        }
        translate([-1, cy_strap - largura_strap/2, espessura_parede_caixa]) cube([largura + 2, largura_strap, altura_strap]); 
        translate([cx_strap - largura_strap/2, -1, espessura_parede_caixa]) cube([largura_strap, comprimento + 2, altura_strap]);

        for(x=pos_x_parafuso, y=pos_y_parafuso)
            translate([x, y, altura_total_base - prof_insercao_m3 + 0.1]) cylinder(h=prof_insercao_m3 + 1, r=diam_insercao_m3/2);
    }
    for(x=pos_x_parafuso, y=pos_y_parafuso)
        translate([x, y, espessura_parede_caixa])
        difference() {
            cylinder(h=altura_interna_base, r=4.5);
            translate([0,0, -1]) cylinder(h=altura_interna_base + 2, r=1.6); 
        }
}

module tampa_intermediaria() {
    translate([0, 0, altura_total_base + 5]) 
    union() {
        difference() {
            hull() {
                for(x=pos_x_parafuso, y=pos_y_parafuso) translate([x, y, 0]) cylinder(h=espessura_tampa_meio, r=raio_canto);
            }
            // Recorte da tela
            translate([largura/2 - tela_l/2 + deslocamento_geral_x + offset_x_tampa_meio, comprimento - tela_c - espessura_parede_caixa - gap_topo_tela, -1])
                cube([tela_l, tela_c, espessura_tampa_meio + 2]);
            
            // Furos Botões
            for(i = [-1:1])
                translate([largura/2 + (i * distancia_entre_botoes) + deslocamento_geral_x, pos_y_botoes_base, -1])
                    cylinder(h=espessura_tampa_meio + 2, r=btn_furo_diam/2);
            
            for(x=pos_x_parafuso, y=pos_y_parafuso) translate([x, y, -1]) cylinder(h=espessura_tampa_meio + 2, r=diam_furo_m3/2);
        }

        // BERÇO DOS BOTÕES (Bosses quadrados)
        for(i = [-1:1]) {
            translate([largura/2 + (i * distancia_entre_botoes) + deslocamento_geral_x, pos_y_botoes_base, -altura_parede_berco])
            difference() {
                translate([-(dim_switch/2 + parede_berco), -(dim_switch/2 + parede_berco), 0])
                    cube([dim_switch + parede_berco*2, dim_switch + parede_berco*2, altura_parede_berco]);
                translate([-dim_switch/2, -dim_switch/2, -0.1])
                    cube([dim_switch, dim_switch, altura_parede_berco + 0.2]);
                translate([0, 0, -1]) cylinder(h=altura_parede_berco + 2, r=btn_furo_diam/2);
            }
        }
    }
}

module moldura_superior_furos_planos() {
    translate([0, 0, altura_total_base + 5 + espessura_tampa_meio + 15]) 
    difference() {
        hull() {
            for(x=pos_x_parafuso, y=pos_y_parafuso) translate([x, y, 0]) cylinder(h=espessura_total_moldura, r=raio_canto);
        }
        translate([0, 0, -0.1])
        hull() {
             for(x=pos_x_parafuso, y=pos_y_parafuso) 
                translate([x, y, 0]) 
                    cylinder(h=altura_interna_moldura + 0.1, r=raio_canto - espessura_parede_moldura);
        }
        translate([largura/2 - moldura_janela_l/2 + moldura_h_offset, 
                   comprimento - moldura_janela_c - moldura_v_offset, -1])
            cube([moldura_janela_l, moldura_janela_c, espessura_total_moldura + 2]);

        for(i = [-1:1])
            translate([largura/2 + (i * distancia_entre_botoes) + deslocamento_geral_x, pos_y_botoes_base, -1])
                cylinder(h=espessura_total_moldura + 2, r=btn_furo_diam/2);

        for(x=pos_x_parafuso, y=pos_y_parafuso) {
            translate([x, y, -1]) cylinder(h=espessura_total_moldura + 2, r=diam_furo_m3/2);
        }
    }
}

module pinos_botoes_anti_rotacao() {
    // Altura total calculada para atravessar o sanduíche
    altura_haste = altura_interna_base + espessura_tampa_meio + espessura_total_moldura + 1.5;
    
    translate([largura + 20, 0, 0])
    for(i = [0:2]) translate([i * 15, 0, 0]) {
        union() {
            // A BASE QUADRADA (O "sapato" que trava no berço)
            translate([-dim_base_pino/2, -dim_base_pino/2, 0])
                cube([dim_base_pino, dim_base_pino, altura_base_pino]);
            
            // A HASTE CILÍNDRICA (O atuador que sai da caixa)
            translate([0, 0, altura_base_pino])
                cylinder(h=altura_haste - altura_base_pino, r=(btn_furo_diam-0.4)/2);
        }
    }
}

// --- RENDER ---
base();
tampa_intermediaria();
moldura_superior_furos_planos();
pinos_botoes_anti_rotacao();