param(
    [string]$Token = $env:GITHUB_TOKEN,
    [string]$Owner = "Rohan-Kadam-code",
    [string]$Repo  = "1DSIM",
    [int]$ProjectNumber = 3
)

if (-not $Token) {
    Write-Error "Set GITHUB_TOKEN environment variable or pass -Token parameter"
    exit 1
}

$headers = @{
    "Authorization"        = "Bearer $Token"
    "Accept"               = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
}

$baseUrl = "https://api.github.com/repos/$Owner/$Repo"

# ---- ISSUE DEFINITIONS -------------------------------------------------------
$issues = @(

    @{
        title  = "[Core] C++ Solver Engine - Conduction, Convection, Radiation"
        body   = "Implemented the base ThermalSystem C++ class with:`n- LINK_CONDUCTION (0): Q = G * dT`n- LINK_CONVECTION (1): Q = hA * dT`n- LINK_RADIATION (2): Q = sigma*eps*A * (T_A^4 - T_B^4)`n`nFiles: src/core/solver.h, src/core/solver.cpp, src/core/bindings.cpp"
        labels = @("core","physics","done")
        status = "Done"
    },
    @{
        title  = "[Core] Fluid Advection Link (LINK_FLOW)"
        body   = "Upstream-biased enthalpy transport:`n- Prescribed volumetric flow rate [L/min]`n- rho(T_up)*cp(T_up) evaluated at upstream node temperature`n- Works with all fluid media`n`nFiles: src/core/solver.cpp - GetLinkFlowRateAndDeriv()"
        labels = @("core","physics","done")
        status = "Done"
    },
    @{
        title  = "[Core] Fan / Pump Link (LINK_FAN) - Analytical Operating Point"
        body   = "Exact analytical intersection of quadratic fan curve with system resistance:`n`nP_fan(v) = P_max - (P_max/v_max^2)*v^2`nP_sys(v) = K*v^2 + R*v`n`nv_oper = (-R + sqrt(R^2 + 4*(K+B)*P_max)) / (2*(K+B))`nQ_vol  = v_oper * A * 60000   [L/min]`n`nParameters: P_max, v_max, K, R, fan_area`nFiles: src/core/solver.cpp"
        labels = @("core","physics","done")
        status = "Done"
    },
    @{
        title  = "[Core] Fluid Media - Water, Glycol, Oil, Air"
        body   = "Temperature-dependent polynomial fluid properties:`n`n| Medium | rho(T) | cp(T) |`n|--------|--------|-------|`n| Water | cubic | quadratic |`n| Glycol 50/50 | linear | linear |`n| Engine Oil | linear | linear |`n| Air | ideal gas | linear |`n`nFiles: src/core/solver.cpp - GetFluidProperties()"
        labels = @("core","fluids","done")
        status = "Done"
    },
    @{
        title  = "[Core] Mixture Fluid - Water-Glycol with Concentration Slider"
        body   = "Linear interpolation between Water and Glycol by user-defined glycol fraction r in [0,1]:`n`nrho_mix  = (1-r)*rho_water(T) + r*rho_glycol(T)`ncp_mix   = (1-r)*cp_water(T)  + r*cp_glycol(T)`n`nAPI: set_node_fluid_params(id, mix_ratio, ...)`nFiles: src/core/solver.cpp, src/core/solver.h"
        labels = @("core","fluids","done")
        status = "Done"
    },
    @{
        title  = "[Core] Custom Fluid - User-Defined Polynomial Coefficients"
        body   = "User specifies quadratic polynomial coefficients for rho(T) and cp(T):`n`nrho(T) = a0 + a1*T + a2*T^2`ncp(T)  = b0 + b1*T + b2*T^2`n`nAPI: set_node_fluid_params(id, 0, rho_a0, rho_a1, rho_a2, cp_a0, cp_a1, cp_a2)`nFiles: src/core/solver.cpp, src/core/solver.h"
        labels = @("core","fluids","done")
        status = "Done"
    },
    @{
        title  = "[Solver] Explicit Euler, RK4, Implicit Backward Euler"
        body   = "Three numerical integration schemes:`n- Explicit Euler: 1st order, fast, conditionally stable`n- RK4: 4th order, default solver, excellent accuracy`n- Implicit Backward Euler: Newton iteration, unconditionally stable, for stiff systems`n`nFiles: src/core/solver.cpp"
        labels = @("core","solver","done")
        status = "Done"
    },
    @{
        title  = "[Solver] Steady-State Newton Iteration"
        body   = "Nonlinear Newton solver for Q_net(T*) = 0.`nConverges in 2-5 iterations for typical thermal networks.`nConfigurable tolerance and max iterations.`n`nAPI: solve_steady_state(tolerance, max_iterations)`nFiles: src/core/solver.cpp"
        labels = @("core","solver","done")
        status = "Done"
    },
    @{
        title  = "[Python] ctypes Wrapper and REST Server"
        body   = "- ThermalSolverWrapper: ctypes bindings for all DLL exports`n- ThermalSystem: high-level Python API with c_to_k / k_to_c helpers`n- set_node_fluid_params(): Mixture/Custom fluid property injection`n- server.py: HTTP REST API with three endpoints:`n  - POST /api/init - initialise system`n  - POST /api/step - advance one time step`n  - POST /api/solve_steady - run steady-state solver`n`nFiles: src/wrapper/thermal_solver.py, src/gui/server.py"
        labels = @("python","api","done")
        status = "Done"
    },
    @{
        title  = "[Web UI] Browser Schematic Editor"
        body   = "- HTML5 Canvas drag-and-drop schematic editor`n- Orthogonal link routing with temperature colour map`n- Animated flow chevrons (direction from velocity sign)`n- Fan/Pump links rendered in violet with impeller icon`n- Attribute Sheet: live editing of all node/link parameters`n- Fluid domain UI: medium dropdown, Mixture slider, Custom polynomial inputs`n- 4 built-in presets: Vehicle, CPU, Battery, Window`n- Dynamic variable sliders per preset`n- Chart.js time plots with series toggles`n- CSV export of full time-history`n`nFiles: src/gui/web/index.html, src/gui/web/app.js, src/gui/web/style.css"
        labels = @("web","ui","done")
        status = "Done"
    },
    @{
        title  = "[Web UI] Fan Matcher Tab - Chart.js Operating Point"
        body   = "When a Fan/Pump link is selected, the Fan Matcher tab appears showing:`n- Blue line: fan curve P_fan(v) = P_max - B*v^2`n- Orange line: system resistance P_sys(v) = K*v^2 + R*v`n- Red dot: operating point (v_oper, P_oper)`n- Metrics: Operating Velocity [m/s], Flow Rate [L/min], Pressure [Pa]`n`nFiles: src/gui/web/app.js - updateFanMatching()"
        labels = @("web","ui","done")
        status = "Done"
    },
    @{
        title  = "[Desktop] Native ImGui + ImPlot + DirectX 11 Application"
        body   = "Native Windows desktop CAD tool:`n- Schematic editor with grid snap, zoom/pan, multi-select`n- Attribute Sheet for all node and link types`n- Fan/Pump properties: P_max, v_max, K, R, fan_area`n- Animated directional flow chevrons at 60 fps`n- Fan links rendered in violet`n- ImPlot Fan Matcher panel with operating point diamond marker`n- .gtm JSON project save/load (backward compatible)`n- build_desktop.py MSVC compilation script`n`nFiles: src/desktop/main.cpp, build_desktop.py"
        labels = @("desktop","ui","done")
        status = "Done"
    },
    @{
        title  = "[Tests] 9 Physics Verification Tests - 0.000000% Error"
        body   = "All tests pass against closed-form analytical solutions:`n`n| Test | Validates |`n|------|-----------|`n| test_fluid_capacities | rho(T)*V*cp(T) for 4 media |`n| test_fluid_advection | Upstream enthalpy transport |`n| test_mixture_fluid | Linear interpolation at 40% glycol |`n| test_custom_fluid | Polynomial rho(T), cp(T) |`n| test_fan_link | Quadratic root v_oper -> heat rate |`n| test_transient_cooling | RK4 exponential decay |`n| test_steady_state | Newton iteration convergence |`n| test_stiff_system_implicit | Implicit Euler stability |`n| test_temperature_dependence | Nonlinear C(T) transient |`n`nFiles: tests/test_simulation.py, tests/test_media_simulation.py"
        labels = @("testing","done")
        status = "Done"
    },
    @{
        title  = "[Docs] README, Architecture, Physics Reference, Changelog"
        body   = "Full project documentation committed to repo:`n- README.md: features, structure, quick-start, physics summary, test table`n- docs/ARCHITECTURE.md: system layers, data flow, component responsibilities`n- docs/PHYSICS.md: complete equations for all link types, fluid media, solvers`n- docs/CHANGELOG.md: full feature list for v1.0.0`n`nRepo: https://github.com/Rohan-Kadam-code/1DSIM"
        labels = @("documentation","done")
        status = "Done"
    },

    @{
        title  = "[Future] Multi-Fluid Loop Support (separate coolant circuits)"
        body   = "Allow multiple independent fluid loops with heat exchangers between them. Currently all fluid nodes share the same medium per node."
        labels = @("enhancement","todo")
        status = "Todo"
    },
    @{
        title  = "[Future] 2D Layout Auto-Router"
        body   = "Automatic hierarchical layout algorithm for schematic nodes when importing from JSON, avoiding overlaps."
        labels = @("enhancement","ui","todo")
        status = "Todo"
    },
    @{
        title  = "[Future] Sensitivity Analysis and Parametric Sweep"
        body   = "Sweep a parameter (e.g. flow rate, conductance) over a range and plot temperature response surface."
        labels = @("enhancement","analysis","todo")
        status = "Todo"
    },
    @{
        title  = "[Future] Linux and macOS Build Support"
        body   = "Currently MSVC-only. Add CMake build for the solver DLL on Linux/macOS. Desktop app uses Dear ImGui which is cross-platform."
        labels = @("build","enhancement","todo")
        status = "Todo"
    }
)

# ---- LABEL CREATION ----------------------------------------------------------
$labelDefs = @(
    @{ name="core";          color="0075ca"; description="C++ solver core" },
    @{ name="physics";       color="e4e669"; description="Physics implementation" },
    @{ name="fluids";        color="0052cc"; description="Fluid media and properties" },
    @{ name="solver";        color="d93f0b"; description="Numerical solver methods" },
    @{ name="python";        color="3776ab"; description="Python wrapper and server" },
    @{ name="api";           color="bfd4f2"; description="REST API" },
    @{ name="web";           color="84b6eb"; description="Browser web interface" },
    @{ name="desktop";       color="a855f7"; description="Native desktop application" },
    @{ name="ui";            color="c5def5"; description="User interface" },
    @{ name="testing";       color="0e8a16"; description="Tests and verification" },
    @{ name="documentation"; color="f9d0c4"; description="Documentation" },
    @{ name="enhancement";   color="a2eeef"; description="Future enhancement" },
    @{ name="build";         color="e4e669"; description="Build system" },
    @{ name="analysis";      color="cc317c"; description="Analysis features" },
    @{ name="done";          color="0e8a16"; description="Completed" },
    @{ name="todo";          color="d93f0b"; description="Planned" }
)

Write-Host "`n=== Creating Labels ===" -ForegroundColor Cyan
foreach ($lbl in $labelDefs) {
    $body = @{ name=$lbl.name; color=$lbl.color; description=$lbl.description } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/labels" -Method POST -Headers $headers -Body $body -ContentType "application/json" -ErrorAction Stop | Out-Null
        Write-Host "  OK  label: $($lbl.name)" -ForegroundColor Green
    } catch {
        if ($_.Exception.Response.StatusCode.value__ -eq 422) {
            Write-Host "  --  label exists: $($lbl.name)" -ForegroundColor Yellow
        } else {
            Write-Host "  ERR label $($lbl.name): $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}

# ---- ISSUE CREATION ----------------------------------------------------------
Write-Host "`n=== Creating Issues ===" -ForegroundColor Cyan
$createdIssues = @()

foreach ($iss in $issues) {
    $bodyText = $iss.body -replace '`n', "`n"
    $payload = @{ title=$iss.title; body=$bodyText; labels=$iss.labels } | ConvertTo-Json
    try {
        $resp = Invoke-RestMethod -Uri "$baseUrl/issues" -Method POST -Headers $headers -Body $payload -ContentType "application/json" -ErrorAction Stop
        Write-Host "  OK  #$($resp.number) - $($iss.title)" -ForegroundColor Green
        $createdIssues += [PSCustomObject]@{ number=$resp.number; node_id=$resp.node_id; title=$iss.title; status=$iss.status }
    } catch {
        Write-Host "  ERR $($iss.title): $($_.Exception.Message)" -ForegroundColor Red
    }
}

# ---- PROJECT BOARD (GraphQL) -------------------------------------------------
Write-Host "`n=== Adding Issues to Project Board ===" -ForegroundColor Cyan

$projectQuery = '{ "query": "query { user(login: \"' + $Owner + '\") { projectV2(number: ' + $ProjectNumber + ') { id fields(first: 20) { nodes { ... on ProjectV2SingleSelectField { id name options { id name } } } } } } }" }'

try {
    $pResp = Invoke-RestMethod -Uri "https://api.github.com/graphql" -Method POST -Headers $headers -Body $projectQuery -ContentType "application/json" -ErrorAction Stop
    $projectId     = $pResp.data.user.projectV2.id
    $statusField   = $pResp.data.user.projectV2.fields.nodes | Where-Object { $_.name -eq "Status" }
    $statusFieldId = $statusField.id
    $doneId        = ($statusField.options | Where-Object { $_.name -eq "Done" }).id
    $todoId        = ($statusField.options | Where-Object { $_.name -eq "Todo" }).id
    $inProgId      = ($statusField.options | Where-Object { $_.name -match "Progress" }).id

    Write-Host "  Project ID   : $projectId" -ForegroundColor Gray
    Write-Host "  Status Field : $statusFieldId" -ForegroundColor Gray
    Write-Host "  Done Option  : $doneId" -ForegroundColor Gray
    Write-Host "  Todo Option  : $todoId" -ForegroundColor Gray

    foreach ($iss in $createdIssues) {
        # Add item to project
        $addQ = '{ "query": "mutation { addProjectV2ItemById(input: { projectId: \"' + $projectId + '\" contentId: \"' + $iss.node_id + '\" }) { item { id } } }" }'
        $addR = Invoke-RestMethod -Uri "https://api.github.com/graphql" -Method POST -Headers $headers -Body $addQ -ContentType "application/json" -ErrorAction Stop
        $itemId = $addR.data.addProjectV2ItemById.item.id

        # Choose status option
        $optId = switch ($iss.status) {
            "Done"        { $doneId }
            "In Progress" { $inProgId }
            default       { $todoId }
        }

        if ($optId -and $statusFieldId -and $itemId) {
            $setQ = '{ "query": "mutation { updateProjectV2ItemFieldValue(input: { projectId: \"' + $projectId + '\" itemId: \"' + $itemId + '\" fieldId: \"' + $statusFieldId + '\" value: { singleSelectOptionId: \"' + $optId + '\" } }) { projectV2Item { id } } }" }'
            Invoke-RestMethod -Uri "https://api.github.com/graphql" -Method POST -Headers $headers -Body $setQ -ContentType "application/json" -ErrorAction Stop | Out-Null
            Write-Host "  OK  board [$($iss.status)] #$($iss.number)" -ForegroundColor Green
        } else {
            Write-Host "  --  item added but status not set for #$($iss.number) (field/option IDs missing)" -ForegroundColor Yellow
        }
    }

} catch {
    Write-Host "`n  WARNING: GraphQL project update failed: $($_.Exception.Message)" -ForegroundColor Yellow
    Write-Host "  Issues were created on the repo. Add them to the board manually at:" -ForegroundColor Yellow
    Write-Host "  https://github.com/users/$Owner/projects/$ProjectNumber" -ForegroundColor Cyan
}

Write-Host "`n=== Complete ===" -ForegroundColor Green
Write-Host "Project board: https://github.com/users/$Owner/projects/$ProjectNumber" -ForegroundColor Cyan
