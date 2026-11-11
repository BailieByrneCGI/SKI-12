param([switch]$Force)

$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$inputDir = [IO.Path]::GetFullPath((Join-Path $workspace 'Tools\Input'))
if (-not $inputDir.StartsWith($workspace.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Input directory must remain in this workspace.'
}
New-Item -ItemType Directory -Path $inputDir -Force | Out-Null

$cases = @(
    @{slug='benefit-auto-rejection'; kind='processing'; issue='automated decisions or profiling'; respondent='Westmere Borough Council'; person='Elena Marsh'; claim='The council rejected my housing support application after an automated risk score marked my earnings as inconsistent, and an officer told me nobody checked the score before the decision was sent.'; harm='My rent payment is at risk.'; outcome='Have a person review the decision and explain the score.'; evidence='The decision notice refers to an automated verification check.'},
    @{slug='health-triage'; kind='processing'; issue='automated decisions or profiling'; respondent='Fenbrook Health Partnership'; person='Priya Hale'; claim='The clinic AI triage portal marked my symptoms low priority and cancelled a same-day appointment without a clinician speaking to me, although I entered information about a chronic condition.'; harm='I waited two days for an assessment.'; outcome='Explain who reviewed the decision and how health data was used.'; evidence='The portal screenshot shows a low-priority result followed by cancellation.'},
    @{slug='recruitment-disability'; kind='processing'; issue='automated decisions or profiling'; respondent='Riverton Staffing Ltd'; person='Marcus Bell'; claim='A recruiter said the AI screening tool inferred a disability from a gap in employment and ranked my application down before anyone offered an interview.'; harm='I lost an interview opportunity.'; outcome='Review the application with a person and explain whether inferred health data was used.'; evidence='The recruiter email refers to an automated suitability score.'},
    @{slug='credit-score-error'; kind='processing'; issue='incorrect personal information'; respondent='Harbourline Finance Ltd'; person='Nadia Rowan'; claim='The lender AI affordability report says I missed three mortgage payments, although I never had a mortgage, and the lender used the report to refuse my credit application.'; harm='The refusal delayed a necessary purchase.'; outcome='Correct the record and reconsider the application.'; evidence='The affordability report lists alleged missed mortgage payments.'},
    @{slug='chatbot-debt-disclosure'; kind='processing'; issue='personal information shared in error'; respondent='Orchard Utilities Ltd'; person='Samir Cole'; claim='The utility chatbot showed a different customer a summary of my bills and claimed I owed money on an account I have never shared with that customer.'; harm='The other customer contacted me about the alleged debt.'; outcome='Correct the account data and explain who saw the billing history.'; evidence='A screenshot of the chatbot answer was sent to me.'},
    @{slug='child-school-profile'; kind='processing'; issue='automated decisions or profiling'; respondent='Moorfield Academy Trust'; person='Olivia Reed'; claim='The school attendance AI combined my child lunch and library records with absence data and labelled the child high risk in a staff meeting.'; harm='Staff now discuss the child differently.'; outcome='Explain why extra records were needed and correct any inaccurate profile.'; evidence='The meeting note lists lunch and library records as model inputs.'},
    @{slug='social-care-training'; kind='processing'; issue='unexpected use of personal information'; respondent='Alderwick County Council'; person='Helen Vale'; claim='The council used historic social care notes to train a case-summary AI through a contractor, but its earlier privacy notice did not mention this use of family records.'; harm='The notes contain sensitive family and health details.'; outcome='Identify the legal basis, contractor role and whether my records were included.'; evidence='The council privacy update describes retrospective use of case notes.'},
    @{slug='support-chat-training'; kind='processing'; issue='unexpected use of personal information'; respondent='Cedar Retail Ltd'; person='Ben Fletcher'; claim='The retailer used old support chats about a faulty order to train an AI sales assistant, but the notice shown when I contacted support described only resolving the order.'; harm='My complaint text may appear in customer-facing answers.'; outcome='Explain the purpose and lawful basis for AI training.'; evidence='The old support transcript and new training notice have different stated purposes.'},
    @{slug='ai-records-access'; kind='access'; issue='sent only some information requested'; respondent='Eastbrook Housing Association'; person='Farah Brooks'; claim='My subject access response included tenancy letters but omitted the tenant-risk AI profile and input records that a caseworker had previously shown me.'; harm='I cannot check whether the profile is accurate.'; outcome='Provide the personal data or explain a lawful reason for withholding it.'; evidence='The access request explicitly named tenant-risk profiles.'},
    @{slug='erasure-training-records'; kind='processing'; issue='wont delete personal information'; respondent='Seabourne Fitness Ltd'; person='Leah Porter'; claim='After the gym agreed to delete old coaching notes, its AI training dashboard still listed those notes as a source and generated a summary referring to an old injury.'; harm='Old health details may affect new recommendations.'; outcome='Explain what remains and assess the erasure request.'; evidence='A dashboard screenshot shows the old notes dataset.'},
    @{slug='marketing-objection'; kind='processing'; issue='marketing after objection'; respondent='Maple Homeware Ltd'; person='Daniel Frost'; claim='I objected to direct marketing, yet the retailer AI continued using my purchase profile to choose and send tailored promotional emails.'; harm='I continue receiving marketing I asked to stop.'; outcome='Stop using my profile for direct marketing.'; evidence='The objection confirmation predates two promotional emails.'},
    @{slug='tenant-ethnicity'; kind='processing'; issue='automated decisions or profiling'; respondent='Fellside Lettings Ltd'; person='Yasmin Clarke'; claim='The letting agent said its AI tenancy score inferred ethnicity from my surname and neighbourhood and marked my application unsuitable before anyone met me.'; harm='I lost the chance to rent the flat.'; outcome='Explain the inputs and review the application with a person.'; evidence='The agent message describes the AI score and lack of manual override.'},
    @{slug='hospital-summary-error'; kind='processing'; issue='wont correct personal information'; respondent='Brookhaven NHS Trust'; person='Ruth Bennett'; claim='The hospital AI discharge summary says I still take a medicine stopped last year, and the records team said it could not correct a generated summary from an archived note.'; harm='A clinician could rely on the outdated medication list.'; outcome='Correct the record and notify relevant clinicians.'; evidence='The medication review records the change but the discharge summary does not.'},
    @{slug='facial-recognition'; kind='processing'; issue='biometric identification'; respondent='Townline Supermarkets Ltd'; person='Owen Grant'; claim='A store manager said facial-recognition cameras compared shoppers to a watch list, and I was stopped after a false match despite seeing only a general CCTV sign at the entrance.'; harm='The stop was distressing and public.'; outcome='Explain the biometric basis, risk assessment and false-match correction.'; evidence='The incident card records a false match.'},
    @{slug='legal-chatbot-leak'; kind='processing'; issue='personal information shared in error'; respondent='Ashfield Legal Services Ltd'; person='Clare Jordan'; claim='The legal AI assistant showed another client part of the medical report I uploaded for my case, and the firm has not explained how the other client retrieved it.'; harm='Private health details were exposed.'; outcome='Contain the disclosure and explain what data was exposed.'; evidence='The firm acknowledged a cross-client AI answer.'},
    @{slug='voice-bot-retention'; kind='processing'; issue='keeps personal information too long'; respondent='Pineway Insurance Ltd'; person='Ibrahim Shaw'; claim='The insurer voice bot retains recordings and transcripts of old claims calls for AI improvement without a stated deletion date, including a call from a claim closed three years ago.'; harm='The recording contains old financial circumstances.'; outcome='Explain retention and remove unnecessary recordings.'; evidence='The privacy page says recordings are kept for AI improvement without a schedule.'},
    @{slug='scraped-cv'; kind='processing'; issue='information collected elsewhere'; respondent='CareerBridge Labs Ltd'; person='Maya Ellis'; claim='A job platform scraped my public CV from another site to train an AI career adviser, but I never dealt with that platform or received a privacy notice about collection.'; harm='I cannot tell what work history was retained.'; outcome='Explain the source, purpose, lawful basis and any notice exception.'; evidence='The platform confirmed it collected the CV from another site.'},
    @{slug='translation-error'; kind='processing'; issue='incorrect personal information'; respondent='Northport Benefits Service'; person='Sofia Moreno'; claim='The service AI translated my statement about temporary work as permanent employment, and the wrong translation was used to suspend my benefit payment.'; harm='The suspension left me unable to meet a bill.'; outcome='Correct the record and reconsider the decision.'; evidence='The original statement and AI translation use different employment terms.'},
    @{slug='insurance-no-review'; kind='processing'; issue='automated decisions or profiling'; respondent='Bracken Mutual Ltd'; person='Peter Cole'; claim='The insurer automatically rejected my flood claim using image analysis, and the rejection letter gave no route to contest the score or request human reconsideration.'; harm='I need a decision to begin home repairs.'; outcome='Provide human review and explain the contest route.'; evidence='The letter refers to image analysis but has no review route.'},
    @{slug='false-conviction-match'; kind='processing'; issue='incorrect personal information'; respondent='Greenbank Community Trust'; person='Tara Mills'; claim='The charity AI background-check tool matched me with another person criminal conviction and marked my volunteer application unsuitable although the birth date in the match report is not mine.'; harm='I was excluded and the false record remains on file.'; outcome='Correct the match, restrict sharing and review the application.'; evidence='The match report shows a different birth date.'}
)

$created = 0
for ($index = 0; $index -lt $cases.Count; $index++) {
    $item = $cases[$index]
    $number = $index + 1
    $filename = '{0:D2}-{1}.json' -f $number, $item.slug
    $path = Join-Path $inputDir $filename
    if ((Test-Path -LiteralPath $path) -and -not $Force) { continue }
    $day = $number + 1
    $form = [ordered]@{
        schema = 'ico_personal_data_complaint_v1'
        case_reference = 'AI-{0:D3}' -f $number
        complaint_type = $item.kind
        issue_types = @($item.issue)
        date_discovered = '2026-10-{0:D2}' -f $day
        date_complained_to_organisation = '2026-10-{0:D2}' -f ($day + 1)
        last_contact_date = '2026-10-{0:D2}' -f ($day + 7)
        respondent = @{name = $item.respondent}
        complainant = @{name = $item.person; acting_on_behalf = ($number -eq 6); person_represented = $(if ($number -eq 6) {'Jamie Reed'} else {''}); under_16 = ($number -eq 6)}
        narrative = $item.claim
        harm = $item.harm
        desired_outcome = $item.outcome
        supporting_evidence = @($item.evidence)
        sharing_restrictions = $(if ($number -eq 6) {'Do not share detailed records with the AI supplier while this is reviewed.'} else {''})
        declaration_agreed = $true
    }
    $json = $form | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText($path, $json + [Environment]::NewLine, (New-Object System.Text.UTF8Encoding($false)))
    $created++
}
Write-Output "Created $created synthetic forms; total expected: $($cases.Count)."
